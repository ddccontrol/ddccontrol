/*
    ddc/ci interface functions
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <string.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

#include "ddcci.h"

#include "config.h"

/* ddc/ci defines */
#define DEFAULT_DDCCI_ADDR	0x37	/* ddc/ci logic sits at 0x37 */
#define DEFAULT_EDID_ADDR	0x50	/* edid sits at 0x50 */

#define DDCCI_COMMAND_READ	0x01	/* read ctrl value */
#define DDCCI_REPLY_READ	0x02	/* read ctrl value reply */
#define DDCCI_COMMAND_WRITE	0x03	/* write ctrl value */

#define DDCCI_COMMAND_SAVE	0x0c	/* save current settings */

#define DDCCI_REPLY_CAPS	0xe3	/* get monitor caps reply */
#define DDCCI_COMMAND_CAPS	0xf3	/* get monitor caps */
#define DDCCI_COMMAND_PRESENCE	0xf7	/* ACCESS.bus presence check */

/* control numbers */
#define DDCCI_CTRL_BRIGHTNESS	0x10

/* samsung specific, magictune starts with writing 1 to this register */
#define DDCCI_CTRL		0xf5
#define DDCCI_CTRL_ENABLE	0x0001
#define DDCCI_CTRL_DISABLE	0x0000

/* ddc/ci iface tunables */
#define MAX_BYTES		127	/* max message length */
#define DELAY   		45000	/* uS to wait after write */

/* magic numbers */
#define MAGIC_1	0x51	/* first byte to send, host address */
#define MAGIC_2	0x80	/* second byte to send, ored with length */
#define MAGIC_XOR 0x50	/* initial xor for received frame */

/* verbosity level (0 - normal, 1 - encoded data, 2 - ddc/ci frames) */
int verbosity = 0;

/* debugging */
void dumphex(FILE *f, unsigned char *buf, unsigned char len)
{
	int i, j;
	
	for (j = 0; j < len; j +=16) {
		if (len > 16) {
			fprintf(f, "%04x: ", j);
		}
		
		for (i = 0; i < 16; i++) {
			if (i + j < len) fprintf(f, "%02x ", buf[i + j]);
			else fprintf(f, "   ");
		}

		fprintf(f, "| ");

		for (i = 0; i < 16; i++) {
			if (i + j < len) fprintf(f, "%c", 
				buf[i + j] >= ' ' && buf[i + j] < 127 ? buf[i + j] : '.');
			else fprintf(f, " ");
		}
		
		fprintf(f, "\n");
	}
}

/* write len bytes (stored in buf) to i2c address addr */
/* return 0 on success, -1 on failure */
int i2c_write(int fd, unsigned int addr, unsigned char *buf, unsigned char len)
{
	int i;
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg             i2cmsg;

	/* done, prepare message */	
	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = addr;
	i2cmsg.flags = 0;
	i2cmsg.len   = len;
	i2cmsg.buf   = buf;

	if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0 )
	{
	    perror("ioctl()");
	    fprintf(stderr,"ioctl returned %d\n",i);
	    return -1;
	}

	return i;

}

/* read at most len bytes from i2c address addr, to buf */
/* return -1 on failure */
int i2c_read(int fd, unsigned int addr, unsigned char *buf, unsigned char len)
{
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg             i2cmsg;
	int i;

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = addr;
	i2cmsg.flags = I2C_M_RD;
	i2cmsg.len   = len;
	i2cmsg.buf   = buf;

	if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0)
	{
	    perror("ioctl()");
	    fprintf(stderr,"ioctl returned %d\n",i);
	    return -1;
	}

	return i;
}

/* stalls execution, allowing write transaction to complete */
void ddcci_delay(struct monitor* mon, int iswrite)
{
	struct timeval now;
	
	if (gettimeofday(&now, NULL)) {
		usleep(DELAY);
	} else {
		if (mon->last.tv_sec >= (now.tv_sec - 1)) {
			unsigned long usec = (now.tv_sec - mon->last.tv_sec) * 10000000 +
				now.tv_usec - mon->last.tv_usec;

			if (usec < DELAY) {
				usleep(DELAY - usec);
				if ((now.tv_usec += (DELAY - usec)) > 1000000) {
					now.tv_usec -= 1000000;
					now.tv_sec++;
				}
			}
		}
		
		if (iswrite) {
			mon->last = now;
		}
	}
}

/* write len bytes (stored in buf) to ddc/ci at address addr */
/* return 0 on success, -1 on failure */
int ddcci_write(struct monitor* mon, unsigned char *buf, unsigned char len)
{
	int i = 0;
	unsigned char _buf[MAX_BYTES + 3];
	unsigned xor = ((unsigned char)mon->addr << 1);	/* initial xor value */

	if (verbosity > 1) {
		fprintf(stderr, "Send: ");
		dumphex(stderr, buf, len);
	}

	/* put first magic */
	xor ^= (_buf[i++] = MAGIC_1);
	
	/* second magic includes message size */
	xor ^= (_buf[i++] = MAGIC_2 | len);
	
	while (len--) /* bytes to send */
		xor ^= (_buf[i++] = *buf++);
		
	/* finally put checksum */
	_buf[i++] = xor;

	/* wait for previous command to complete */
	ddcci_delay(mon, 1);

	return i2c_write(mon->fd, mon->addr, _buf, i);
}

/* read ddc/ci formatted frame from ddc/ci at address addr, to buf */
int ddcci_read(struct monitor* mon, unsigned char *buf, unsigned char len)
{
	unsigned char _buf[MAX_BYTES];
	unsigned char xor = MAGIC_XOR;
	int i, _len;

	/* wait for previous command to complete */
	ddcci_delay(mon, 0);

	if (i2c_read(mon->fd, mon->addr, _buf, len + 3) <= 0) /* busy ??? */
	{
		return -1;
	}
	
	/* validate answer */
	if (_buf[0] != mon->addr * 2) { /* busy ??? */
		if (verbosity) {
			fprintf(stderr, "Invalid response, first byte is 0x%02x, should be 0x%02x\n",
				_buf[0], mon->addr * 2);
			dumphex(stderr, _buf, len + 3);
		}
		return -1;
	}

	if ((_buf[1] & MAGIC_2) == 0) {
		fprintf(stderr, "Invalid response, magic is 0x%02x\n", _buf[1]);
		return -1;
	}

	_len = _buf[1] & ~MAGIC_2;
	if (_len > len || _len > sizeof(_buf)) {
		fprintf(stderr, "Invalid response, length is %d, should be %d at most\n",
			_len, len);
		return -1;
	}

	/* get the xor value */
	for (i = 0; i < _len + 3; i++) {
		xor ^= _buf[i];
	}
	
	if (xor != 0) {
		fprintf(stderr, "Invalid response, corrupted data - xor is 0x%02x, length 0x%02x\n", xor, _len);
		dumphex(stderr, _buf, len + 3);
		
		return -1;
	}

	/* copy payload data */
	memcpy(buf, _buf + 2, _len);
	
	if (verbosity > 1) {
		fprintf(stderr, "Recv: ");
		dumphex(stderr, buf, _len);
	}
	
	return _len;
}

/* write value to register ctrl of ddc/ci at address addr */
int ddcci_writectrl(struct monitor* mon, unsigned char ctrl, unsigned short value)
{
	unsigned char buf[4];

	buf[0] = DDCCI_COMMAND_WRITE;
	buf[1] = ctrl;
	buf[2] = (value >> 8);
	buf[3] = (value & 255);

	return ddcci_write(mon, buf, sizeof(buf));
}

/* read register ctrl raw data of ddc/ci at address addr */
int ddcci_raw_readctrl(struct monitor* mon, 
	unsigned char ctrl, unsigned char *buf, unsigned char len)
{
	unsigned char _buf[2];

	_buf[0] = DDCCI_COMMAND_READ;
	_buf[1] = ctrl;

	if (ddcci_write(mon, _buf, sizeof(_buf)) < 0)
	{
		return -1;
	}

	return ddcci_read(mon, buf, len);
}

int ddcci_readctrl(struct monitor* mon, unsigned char ctrl, int force, struct control_ret* ctrl_ret)
{
	unsigned char buf[8];

	int len = ddcci_raw_readctrl(mon, ctrl, buf, sizeof(buf));
	
	ctrl_ret->supported = 0;
	ctrl_ret->value     = 0;
	ctrl_ret->maximum   = 0;
	
	if (len == sizeof(buf) && buf[0] == DDCCI_REPLY_READ &&
		buf[2] == ctrl && (force || !buf[1])) /* buf[1] is validity (0 - valid, 1 - invalid) */
	{	
		ctrl_ret->supported = !buf[1];
		ctrl_ret->value   = buf[6] * 256 + buf[7];
		ctrl_ret->maximum = buf[4] * 256 + buf[5];
		
		if (verbosity > 0) {
			fprintf(stdout, "Control 0x%02x: %c/%d/%d\n", ctrl, 
				ctrl_ret->supported ? '+' : '-',  ctrl_ret->value, ctrl_ret->maximum);
			if (verbosity > 1) {
				fprintf(stderr, "Raw: ");
				dumphex(stderr, buf, sizeof(buf));
			}
		}
	}
	
	return len;
}

/* read capabilities raw data of ddc/ci at address addr starting at offset to buf */
int ddcci_raw_caps(struct monitor* mon, unsigned int offset, unsigned char *buf, unsigned char len)
{
	unsigned char _buf[3];

	_buf[0] = DDCCI_COMMAND_CAPS;
	_buf[1] = offset >> 8;
	_buf[2] = offset & 255;

	if (ddcci_write(mon, _buf, sizeof(_buf)) < 0) 
	{
		return -1;
	}
	
	return ddcci_read(mon, buf, len);
}

int ddcci_caps(struct monitor* mon, unsigned char *buffer, unsigned int buflen) {
	if (verbosity) {
		fprintf(stdout, "\nCapabilities:\n");
	}
	
	int bufferpos = 0;
	unsigned char buf[35];	/* 19 bytes chunk */
	int offset = 0;
	int len, i;
	
	buf[bufferpos] = 0;
	
	do {
		len = ddcci_raw_caps(mon, offset, buf, sizeof(buf));
		if (len < 0) {
			return -1;
		}
		
		if (len < 3 || buf[0] != DDCCI_REPLY_CAPS || (buf[1] * 256 + buf[2]) != offset) 
		{
			fprintf(stderr, "Invalid sequence in caps.\n");
			return -1;
		}

		for (i = 3; i < len; i++) {
			buffer[bufferpos++] = buf[i];
			if (verbosity) {
				fprintf(stdout, i > 2 && buf[i] >= 0x20 && buf[i] < 127 ? "%c" : "0x%02x ", buf[i]);
			}
			if (bufferpos >= buflen) {
				fprintf(stderr, "Buffer too small to contain caps.\n");
				return -1;
			}
		}
		buffer[bufferpos] = 0;

		offset += len - 3;
	} while (len > 3);
	
	if (verbosity) {
		fprintf(stdout, "\n");
	}
	
	return bufferpos;
}

/* save current settings */
int ddcci_command(struct monitor* mon, unsigned char cmd)
{
	unsigned char _buf[1];

	_buf[0] = cmd;

	return ddcci_write(mon, _buf, sizeof(_buf));
}

int ddcci_read_edid(struct monitor* mon, int addr) {
	unsigned char buf[128];
	buf[0] = 0;	/* eeprom offset */
	
	if (verbosity) {
		printf("\nReading EDID : 0x%02x\n", addr);
	}
	if (i2c_write(mon->fd, addr, buf, 1) > 0 &&
		i2c_read(mon->fd, addr, buf, sizeof(buf)) > 0) {
		if (verbosity) {
			dumphex(stdout, buf, sizeof(buf));
		}
		
		snprintf(mon->pnpid, 8, "%c%c%c%02X%02X", 
				((buf[8] >> 2) & 31) + 'A' - 1, 
				((buf[8] & 3) << 3) + (buf[9] >> 5) + 'A' - 1, 
				(buf[9] & 31) + 'A' - 1, 
				buf[11], buf[10]);
		mon->digital = (buf[20] & 0x80);
		
		if (verbosity) {
			printf("\tPlug and Play ID: %s\n", mon->pnpid);
			printf("\tInput type: %s\n", mon->digital ? "Digital" : "Analog");
		}
		
		return 0;
	} 
	else {
		fprintf(stderr, "Reading EDID 0x%02x failed.\n", addr);
		return -1;
	}
}

/* Returns :
  - 0 if OK
  - -1 if DDC/CI is not available
  - -2 if EDID is not available
  - -3 if file can't be opened 
*/
int ddcci_open_with_addr(struct monitor* mon, const char* filename, int addr, int edid) {
	if (verbosity) {
		fprintf(stdout, "\nUsing ddc/ci : 0x%02x@%s\n", addr, filename);
	}

	if ((mon->fd = open(filename, O_RDWR)) < 0) {
		perror(filename);
		fprintf(stderr, "Be sure you've modprobed i2c-dev and correct framebuffer device.\n");
		return -3;
	}
	
	mon->addr = addr;
	
	if (ddcci_read_edid(mon, edid) < 0) {
		return -2;
	}
	
	mon->db = ddcci_create_db(mon->pnpid);
	
	if (mon->db) {
		if (mon->db->init == samsung) {
			if (ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_ENABLE) < 0) {
				return -1;
			}
		}
		else {
			if (ddcci_command(mon, DDCCI_COMMAND_PRESENCE) < 0) {
				return -1;
			}
		}
	}
	else { /* Alternate way of init mode detecting for unsupported monitors */
		if (strncmp(mon->pnpid, "SAM", 3) == 0) {
			if (ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_ENABLE) < 0) {
				return -1;
			}
		}
		else {
			if (ddcci_command(mon, DDCCI_COMMAND_PRESENCE) < 0) {
				return -1;
			}
		}
	}
	
	return 0;
}

int ddcci_open(struct monitor* mon, const char* filename) {
	return ddcci_open_with_addr(mon, filename, DEFAULT_DDCCI_ADDR, DEFAULT_EDID_ADDR);
}

int ddcci_save(struct monitor* mon) {
	if (verbosity) {
		fprintf(stdout, "\nSaving settings...\n");
	}
	return ddcci_command(mon, DDCCI_COMMAND_SAVE);
}

/* Returns :
  - 0 if OK
  - -1 if DDC/CI is not available
  - -3 if file can't be closed 
*/
int ddcci_close(struct monitor* mon) {
	/* TODO: Read database to know if we should use Samsung mode */
	
	if (strncmp(mon->pnpid, "SAM", 3) == 0) {
		if ((ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_DISABLE)) < 0) {
			return -1;
		}
	}
	
	if (close(mon->fd) < 0) {
		return -3;
	}
	
	return 0;
}
