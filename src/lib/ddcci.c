/*
    ddc/ci interface functions
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2006 Nicolas Boichat (nicolas@boichat.ch)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "config.h"

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#ifdef HAVE_I2C_DEV
#include "i2c-dev.h"
#endif
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "ddcci.h"
#include "internal.h"
#include "amd_adl.h"

#include "conf.h"

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

#define CONTROL_WRITE_DELAY   80000	/* uS to wait after writing to a control (default) */

/* magic numbers */
#define MAGIC_1	0x51	/* first byte to send, host address */
#define MAGIC_2	0x80	/* second byte to send, ored with length */
#define MAGIC_XOR 0x50	/* initial xor for received frame */

/* verbosity level (0 - normal, 1 - encoded data, 2 - ddc/ci frames) */
static int verbosity = 0;

void ddcci_verbosity(int _verbosity)
{
	verbosity = _verbosity;
}

int get_verbosity() {
	return verbosity;
}

/* debugging */
static void dumphex(FILE *f, char *text, unsigned char *buf, int len)
{
	int i, j;

	if (text) {
		if (len > 16) {
			fprintf(f, "%s:\n", text);
		}
		else {
			fprintf(f, "%s: ", text);
		}
	}

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

/* IPC functions */
#ifdef HAVE_DDCPCI
#include "ddcpci-ipc.h"
#include <sys/msg.h>
#include <sys/ipc.h>

#define DDCPCI_RETRY_DELAY 10000 /* in us */
#define DDCPCI_RETRIES 100000

static int msqid = -2;

int ddcpci_init()
{
	if (msqid == -2) {
		if (verbosity) {
			printf("ddcpci initing...\n");
		}
		
		key_t key = ftok(DDCPCIDIR "/ddcpci", getpid());
		
		if ((msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
			perror(_("Error while initialisating the message queue"));
			return 0;
		}
		
		char buffer[256];
		
		snprintf(buffer, 256, DDCPCIDIR "/ddcpci %d %d &", verbosity, key);
		
		if (verbosity) {
			printf("Starting %s...\n", buffer);
		}
		
		system(buffer);
	}
	return (msqid >= 0);
}

void ddcpci_release()
{
	if (verbosity) {
		printf("ddcpci being released...\n");
	}
	if (msqid >= 0) {
		struct query qlist;
		memset(&qlist, 0, sizeof(struct query));
		qlist.mtype = 1;
		qlist.qtype = QUERY_QUIT;
		
		if (msgsnd(msqid, &qlist, QUERY_SIZE, IPC_NOWAIT) < 0) {
			perror(_("Error while sending quit message"));
		}
		
		usleep(20000);
		
		msgctl(msqid, IPC_RMID, NULL);
	}
}

/* Returns : 0 - OK, negative value - timed out or another error */
int ddcpci_read(struct answer* manswer)
{
	int i, ret;
	for (i = DDCPCI_RETRIES;; i--) {
		if ((ret = msgrcv(msqid, manswer, sizeof(struct answer) - sizeof(long), 2, IPC_NOWAIT)) < 0) {
			if (errno != ENOMSG) {
				return -errno;
			}
		}
		else {
			if (manswer->status < 0) {
				errno = EBADMSG;
				return -EBADMSG;
			}
			else {
				return ret;
			}
		}
		
		if (i == 0) {
			errno = ETIMEDOUT;
			return -ETIMEDOUT;
		}
		usleep(DDCPCI_RETRY_DELAY);
	}
}

/* Send heartbeat so ddcpci doesn't timeout */
void ddcpci_send_heartbeat() {
	if (msqid >= 0) {
		struct query qheart;
		memset(&qheart, 0, sizeof(struct query));
		qheart.mtype = 1;
		qheart.qtype = QUERY_HEARTBEAT;
		
		if (msgsnd(msqid, &qheart, QUERY_SIZE, IPC_NOWAIT) < 0) {
			perror(_("Error while sending heartbeat message"));
		}
	}
}

#else
int ddcpci_init() {
	return 1;
}

void ddcpci_release() {}

void ddcpci_send_heartbeat() {}
#endif

int ddcci_init(char* usedatadir)
{
	if (!ddcci_init_db(usedatadir)) {
		printf(_("Failed to initialize ddccontrol database...\n"));
		return 0;
	}
#ifdef HAVE_AMDADL
	if (!amd_adl_init()){
		if (verbosity) {
			printf(_("Failed to initialize ADL...\n"));
		}
	}
#endif
	return ddcpci_init();
}

void ddcci_release() {
	ddcpci_release();
	ddcci_release_db();
#ifdef HAVE_AMDADL
	amd_adl_free();
#endif
}

/* write len bytes (stored in buf) to i2c address addr */
/* return 0 on success, -1 on failure */
static int i2c_write(struct monitor* mon, unsigned int addr, unsigned char *buf, unsigned char len)
{
	switch (mon->type) {
#ifdef HAVE_I2C_DEV
	case dev:
	{	
		int i;
		struct i2c_rdwr_ioctl_data msg_rdwr;
		struct i2c_msg             i2cmsg;
	
		memset(&msg_rdwr, 0, sizeof(msg_rdwr));
		memset(&i2cmsg, 0, sizeof(i2cmsg));
		msg_rdwr.msgs = &i2cmsg;
		msg_rdwr.nmsgs = 1;
	
#ifdef __FreeBSD__
		i2cmsg.slave = addr << 1;
#else
		i2cmsg.addr  = addr;
#endif
		i2cmsg.flags = 0;
		i2cmsg.len   = len;
		i2cmsg.buf   = buf;
	
		if ((i = ioctl(mon->fd, I2C_RDWR, &msg_rdwr)) < 0 )
		{
			if (!mon->probing || verbosity) {
				perror("ioctl()");
				fprintf(stderr,_("ioctl returned %d\n"),i);
			}
			return -1;
		}

		if (verbosity > 1) {
			dumphex(stderr, "Send", buf, len);
		}

#ifdef __FreeBSD__
		i = len; // FreeBSD ioctl() returns 0
#endif

		return i;
	}
#endif
#ifdef HAVE_DDCPCI
	case pci:
	{
		struct query qdata;
		memset(&qdata, 0, sizeof(struct query));
		qdata.mtype = 1;
		qdata.qtype = QUERY_DATA;
		qdata.addr = addr;
		qdata.flags = 0;
		memcpy(qdata.buffer, buf, len);
		
		if (msgsnd(msqid, &qdata, QUERY_SIZE + len, IPC_NOWAIT) < 0) {
			if (!mon->probing || verbosity) {
				perror(_("Error while sending write message"));
			}
			return -3;
		}
		
		struct answer adata;
		
		if (ddcpci_read(&adata) < 0) {
			if (!mon->probing || verbosity) {
				perror(_("Error while reading write message answer"));
			}
			return -1;
		}

		if (verbosity > 1) {
			dumphex(stderr, "Send", buf, len);
		}

		return adata.status;
	}
#endif
#ifdef HAVE_AMDADL
	case type_adl:
	{
		return amd_adl_i2c_write(mon->adl_adapter, mon->adl_display, addr, buf, len);
	}
#endif
	default:
		return -1;
	}
}

/* read at most len bytes from i2c address addr, to buf */
/* return -1 on failure */
static int i2c_read(struct monitor* mon, unsigned int addr, unsigned char *buf, unsigned char len)
{
	switch (mon->type) {
#ifdef HAVE_I2C_DEV
	case dev:
	{
		struct i2c_rdwr_ioctl_data msg_rdwr;
		struct i2c_msg             i2cmsg;
		int i;
	
		memset(&msg_rdwr, 0, sizeof(msg_rdwr));
		memset(&i2cmsg, 0, sizeof(i2cmsg));
		msg_rdwr.msgs = &i2cmsg;
		msg_rdwr.nmsgs = 1;
	
#ifdef __FreeBSD__
		i2cmsg.slave = addr << 1;
#else
		i2cmsg.addr  = addr;
#endif
		i2cmsg.flags = I2C_M_RD;
		i2cmsg.len   = len;
		i2cmsg.buf   = buf;
	
		if ((i = ioctl(mon->fd, I2C_RDWR, &msg_rdwr)) < 0)
		{
			if (!mon->probing || verbosity) {
				perror("ioctl()");
				fprintf(stderr,_("ioctl returned %d\n"),i);
			}
			return -1;
		}

		if (verbosity > 1) {
			dumphex(stderr, "Recv", buf, i);
		}

#ifdef __FreeBSD__
		i = len; // FreeBSD ioctl() returns 0
#endif

		return i;
	}
#endif
#ifdef HAVE_DDCPCI
	case pci:
	{
		int ret;
		struct query qdata;
		memset(&qdata, 0, sizeof(struct query));
		qdata.mtype = 1;
		qdata.qtype = QUERY_DATA;
		qdata.addr = addr;
		qdata.flags = I2C_M_RD;
		qdata.len = len;
		
		if (msgsnd(msqid, &qdata, QUERY_SIZE, IPC_NOWAIT) < 0) {
			if (!mon->probing || verbosity) {
				perror(_("Error while sending read message"));
			}
			return -3;
		}
		
		struct answer adata;
		
		if ((ret = ddcpci_read(&adata)) < 0) {
			if (!mon->probing || verbosity) {
				perror(_("Error while reading read message answer"));
			}
			return -1;
		}
		
		memcpy(buf, adata.buffer, ret - ANSWER_SIZE);
		
		if (verbosity > 1) {
			dumphex(stderr, "Recv", buf, ret - ANSWER_SIZE);
		}

		return ret - ANSWER_SIZE;
	}
#endif
#ifdef HAVE_AMDADL
	case type_adl:
	{
		return amd_adl_i2c_read(mon->adl_adapter, mon->adl_display, addr, buf, len);
	}
#endif
	default:
		return -1;
	}
}

/* stalls execution, allowing write transaction to complete */
static void ddcci_delay(struct monitor* mon, int iswrite)
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
static int ddcci_write(struct monitor* mon, unsigned char *buf, unsigned char len)
{
	int i = 0;
	unsigned char _buf[MAX_BYTES + 3];
	unsigned xor = ((unsigned char)mon->addr << 1);	/* initial xor value */

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

	return i2c_write(mon, mon->addr, _buf, i);
}

/* read ddc/ci formatted frame from ddc/ci at address addr, to buf */
static int ddcci_read(struct monitor* mon, unsigned char *buf, unsigned char len)
{
	unsigned char _buf[MAX_BYTES];
	unsigned char xor = MAGIC_XOR;
	int i, _len;

	/* wait for previous command to complete */
	ddcci_delay(mon, 0);

	if (i2c_read(mon, mon->addr, _buf, len + 3) <= 0) /* busy ??? */
	{
		return -1;
	}
	
	/* validate answer */
	if (_buf[0] != mon->addr * 2) { /* busy ??? */
		if (!mon->probing || verbosity) {
			fprintf(stderr, _("Invalid response, first byte is 0x%02x, should be 0x%02x\n"),
				_buf[0], mon->addr * 2);
			dumphex(stderr, NULL, _buf, len + 3);
		}
		return -1;
	}

	if ((_buf[1] & MAGIC_2) == 0) {
		/* Fujitsu Siemens P19-2 and NEC LCD 1970NX send wrong magic when reading caps. */
		if (!mon->probing || verbosity) {
			fprintf(stderr, _("Non-fatal error: Invalid response, magic is 0x%02x\n"), _buf[1]);
		}
	}

	_len = _buf[1] & ~MAGIC_2;
	if (_len > len || _len > sizeof(_buf)) {
		if (!mon->probing || verbosity) {
			fprintf(stderr, _("Invalid response, length is %d, should be %d at most\n"),
				_len, len);
		}
		return -1;
	}

	/* get the xor value */
	for (i = 0; i < _len + 3; i++) {
		xor ^= _buf[i];
	}
	
	if (xor != 0) {
		if (!mon->probing || verbosity) {
			fprintf(stderr, _("Invalid response, corrupted data - xor is 0x%02x, length 0x%02x\n"), xor, _len);
			dumphex(stderr, NULL, _buf, len + 3);
		}
		
		return -1;
	}

	/* copy payload data */
	memcpy(buf, _buf + 2, _len);
		
	return _len;
}

/* write value to register ctrl of ddc/ci at address addr */
int ddcci_writectrl(struct monitor* mon, unsigned char ctrl, unsigned short value, int delay)
{
	if(mon->__vtable) {
		return mon->__vtable->writectrl(mon, ctrl, value, delay);
	}

	unsigned char buf[4];

	buf[0] = DDCCI_COMMAND_WRITE;
	buf[1] = ctrl;
	buf[2] = (value >> 8);
	buf[3] = (value & 255);

	int ret = ddcci_write(mon, buf, sizeof(buf));
	
	/* Do the delay */
	if (delay > 0) {
		usleep(1000*delay);
	}
	/* Default delay : 80ms (anyway we won't get below 45ms (due to DELAY)) */
	else if (delay < 0) {
		usleep(CONTROL_WRITE_DELAY);
	}
	
	return ret;
}

/* read register ctrl raw data of ddc/ci at address addr */
static int ddcci_raw_readctrl(struct monitor* mon, 
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

int ddcci_readctrl(struct monitor* mon, unsigned char ctrl, 
	unsigned short *value, unsigned short *maximum)
{
	if(mon->__vtable) {
		return mon->__vtable->readctrl(mon, ctrl, value, maximum);
	}

	unsigned char buf[8];

	int len = ddcci_raw_readctrl(mon, ctrl, buf, sizeof(buf));
	
	if (len == sizeof(buf) && buf[0] == DDCCI_REPLY_READ &&	buf[2] == ctrl) 
	{	
		if (value) {
			*value = buf[6] * 256 + buf[7];
		}
		
		if (maximum) {
			*maximum = buf[4] * 256 + buf[5];
		}
		
		return !buf[1];
		
	}
	
	return -1;
}

/* See documentation Appendix D.
 * Returns :
 * -1 if an error occurred 
 *  number of controls added
 *
 * add: if true: add caps_str to caps, otherwise remove caps_str from the caps.
 */
int ddcci_parse_caps(const char* caps_str, struct caps* caps, int add)
{
//	printf("Parsing CAPS (%s).\n", caps_str);
	int pos = 0; /* position in caps_str */
	
	int level = 0; /* CAPS parenthesis level */
	int svcp = 0; /* Current CAPS section is vcp */
	int stype = 0; /* Current CAPS section is type */
	
	char buf[128];
	char* endptr;
	int ind = -1;
	long val = -1;
	int i;
	int removeprevious = 0;
	
	int num = 0;
	
	for (pos = 0; caps_str[pos] != 0; pos++)
	{
		if (caps_str[pos] == '(') {
			level++;
		}
		else if (caps_str[pos] == ')')
		{
			level--;
			if (level == 1) {
				svcp = 0;
				stype = 0;
			}
		}
		else if (caps_str[pos] != ' ')
		{
			if (level == 1) {
				if ((strncmp(caps_str+pos, "vcp(", 4) == 0) || (strncmp(caps_str+pos, "vcp ", 4) == 0)) {
					svcp = 1;
					pos += 2;
				}
				else if (strncmp(caps_str+pos, "type", 4) == 0) {
					stype = 1;
					pos += 3;
				}
			}
			else if ((stype == 1) && (level == 2)) {
				if (strncmp(caps_str+pos, "lcd", 3) == 0) {
					caps->type = lcd;
					pos += 2;
				}
				else if (strncmp(caps_str+pos, "crt", 3) == 0) {
					caps->type = crt;
					pos += 2;
				}
			}
			else if ((svcp == 1) && (level == 2)) {
				if (!add && ((removeprevious == 1) || (caps->vcp[ind] && caps->vcp[ind]->values_len == 0))) {
					if(caps->vcp[ind]) {
						if (caps->vcp[ind]->values) {
							free(caps->vcp[ind]->values);
						}
						free(caps->vcp[ind]);
						caps->vcp[ind] = NULL;
					}
				}
				buf[0] = caps_str[pos];
				buf[1] = caps_str[++pos];
				buf[2] = 0;
				ind = strtol(buf, &endptr, 16);
				if (*endptr != 0) {
					printf(_("Can't convert value to int, invalid CAPS (buf=%s, pos=%d).\n"), buf, pos);
					return -1;
				}
				if (add) {
					caps->vcp[ind] = malloc(sizeof(struct vcp_entry));
					caps->vcp[ind]->values_len = -1;
					caps->vcp[ind]->values = NULL;
				}
				else {
					removeprevious = 1;
				}
				num++;
			}
			else if ((svcp == 1) && (level == 3)) {
				i = 0;
				while ((caps_str[pos+i] != ' ') && (caps_str[pos+i] != ')')) {
					buf[i] = caps_str[pos+i];
					i++;
				}
				buf[i] = 0;
				val = strtol(buf, &endptr, 16);
				if (*endptr != 0) {
					printf(_("Can't convert value to int, invalid CAPS (buf=%s, pos=%d).\n"), buf, pos);
					return -1;
				}
				if (add) {
					if (caps->vcp[ind]->values_len == -1) {
						caps->vcp[ind]->values_len = 1;
					}
					else {
						caps->vcp[ind]->values_len++;
					}
					caps->vcp[ind]->values = realloc(caps->vcp[ind]->values, caps->vcp[ind]->values_len*sizeof(unsigned short));
					caps->vcp[ind]->values[caps->vcp[ind]->values_len-1] = val;
				}
				else {
					if (caps->vcp[ind]->values_len > 0) {
						removeprevious = 0;
						int j = 0;
						for (i = 0; i < caps->vcp[ind]->values_len; i++) {
							if (caps->vcp[ind]->values[i] != val) {
								caps->vcp[ind]->values[j++] = caps->vcp[ind]->values[i];
							}
						}
						caps->vcp[ind]->values_len--;
					}
				}
			}
		}
	}
	
	if (!add && ((removeprevious == 1) || (caps->vcp[ind] && caps->vcp[ind]->values_len == 0))) {
		if(caps->vcp[ind]) {
			if (caps->vcp[ind]->values) {
				free(caps->vcp[ind]->values);
			}
			free(caps->vcp[ind]);
			caps->vcp[ind] = NULL;
		}
	}
	
	return num;
}

/* read capabilities raw data of ddc/ci at address addr starting at offset to buf */
static int ddcci_raw_caps(struct monitor* mon, unsigned int offset, unsigned char *buf, unsigned char len)
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

int ddcci_caps(struct monitor* mon)
{
	mon->caps.raw_caps = (char*)malloc(16);
	int bufferpos = 0;
	unsigned char buf[64];	/* 64 bytes chunk (was 35, but 173P+ send 43 bytes chunks) */
	int offset = 0;
	int len, i;
	int retries = 3;
	
	do {
		mon->caps.raw_caps[bufferpos] = 0;
		if (retries == 0) {
			return -1;
		}
		
		len = ddcci_raw_caps(mon, offset, buf, sizeof(buf));
		if (len < 0) {
			retries--;
			continue;
		}
		
		if (len < 3 || buf[0] != DDCCI_REPLY_CAPS || (buf[1] * 256 + buf[2]) != offset) 
		{
			if (!mon->probing || verbosity) {
				fprintf(stderr, _("Invalid sequence in caps.\n"));
			}
			retries--;
			continue;
		}

		mon->caps.raw_caps = (char*)realloc(mon->caps.raw_caps, bufferpos + len - 2);
		for (i = 3; i < len; i++) {
			mon->caps.raw_caps[bufferpos++] = buf[i];
		}
		
		offset += len - 3;
		
		retries = 3;
	} while (len != 3);

#if 0
	/* Test CAPS with binary data */
	mon->caps.raw_caps = realloc(mon->caps.raw_caps, 2048);
	strcpy(mon->caps.raw_caps, "( prot(monitor) type(crt) edid bin(128(");
	bufferpos = strlen(mon->caps.raw_caps);
	for (i = 0; i < 128; i++) {
		mon->caps.raw_caps[bufferpos++] = i;
	}
	strcpy(&mon->caps.raw_caps[bufferpos], ")) vdif bin(128(");
	bufferpos += strlen(")) vdif bin(128(");	
	for (i = 0; i < 128; i++) {
		mon->caps.raw_caps[bufferpos++] = i;
	}
	strcpy(&mon->caps.raw_caps[bufferpos], ")) vcp (10 12 16 18 1A 50 92)))");
	bufferpos += strlen(")) vcp (10 12 16 18 1A 50 92)))");
	/* End */
#endif
	
	mon->caps.raw_caps[bufferpos] = 0;

	char* last_substr = mon->caps.raw_caps;
	char* endptr;
	while ((last_substr = strstr(last_substr, "bin("))) {
		last_substr += 4;
		len = strtol(last_substr, &endptr, 0);
		if (*endptr != '(') {
			printf("Invalid bin in CAPS.\n");
			continue;
		}
		for (i = 0; i < len; i++) {
			*(++endptr) = '#';
		}
		last_substr += len;
	}
	
	ddcci_parse_caps(mon->caps.raw_caps, &mon->caps, 1);
	
	return bufferpos;
}

/* save current settings */
int ddcci_command(struct monitor* mon, unsigned char cmd)
{
	unsigned char _buf[1];

	_buf[0] = cmd;

	return ddcci_write(mon, _buf, sizeof(_buf));
}

int ddcci_read_edid(struct monitor* mon, int addr) 
{
	unsigned char buf[128];
	buf[0] = 0;	/* eeprom offset */
	
	if (i2c_write(mon, addr, buf, 1) > 0 &&
	    i2c_read(mon, addr, buf, sizeof(buf)) > 0) 
	{		
		if (buf[0] != 0 || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff ||
		    buf[4] != 0xff || buf[5] != 0xff || buf[6] != 0xff || buf[7] != 0)
		{
			if (!mon->probing || verbosity) {
				fprintf(stderr, _("Corrupted EDID at 0x%02x.\n"), addr);
			}
			return -1;
		}
		
		snprintf(mon->pnpid, 8, "%c%c%c%02X%02X", 
			((buf[8] >> 2) & 31) + 'A' - 1, 
			((buf[8] & 3) << 3) + (buf[9] >> 5) + 'A' - 1, 
			(buf[9] & 31) + 'A' - 1, buf[11], buf[10]);
		
		if (!mon->probing && verbosity) {
			int sn = buf[0xc] + (buf[0xd]<<8) + (buf[0xe]<<16) + (buf[0xf]<<24);
			printf(_("Serial number: %d\n"), sn);
			int week = buf[0x10];
			int year = buf[0x11] + 1990;
			printf(_("Manufactured: Week %d, %d\n"), week, year);
			int ver = buf[0x12];
			int rev = buf[0x13];
			printf(_("EDID version: %d.%d\n"), ver, rev);
			int maxwidth = buf[0x15];
			int maxheight = buf[0x16];
			printf(_("Maximum size: %d x %d (cm)\n"), maxwidth, maxheight);
			
			/* Parse more infos... */
		}
		
		mon->digital = (buf[0x14] & 0x80);
		
		return 0;
	} 
	else {
		if (!mon->probing || verbosity) {
			fprintf(stderr, _("Reading EDID 0x%02x failed.\n"), addr);
		}
		return -1;
	}
}

/* Param probing indicates if we are probing for available devices (so we must be much less verbose)
   Returns :
  - 0 if OK
  - -1 if DDC/CI is not available
  - -2 if EDID is not available
  - -3 if file can't be opened 
*/
static int ddcci_open_with_addr(struct monitor* mon, const char* filename, int addr, int edid, int probing) 
{
	memset(mon, 0, sizeof(struct monitor));
	
	mon->probing = probing;
	
	if (strncmp(filename, "dev:", 4) == 0) {
		if ((mon->fd = open(filename+4, O_RDWR)) < 0) {
			if ((!probing) || verbosity)
				perror(filename);
			return -3;
		}
		mon->type = dev;
	}
#ifdef HAVE_DDCPCI
	else if (strncmp(filename, "pci:", 4) == 0) {
		if (verbosity)
			printf(_("Device: %s\n"), filename);
		
		struct query qopen;
		memset(&qopen, 0, sizeof(struct query));
		qopen.mtype = 1;
		qopen.qtype = QUERY_OPEN;
		
		sscanf(filename, "pci:%02x:%02x.%d-%d\n", 
			(unsigned int*)&qopen.bus.bus,
			(unsigned int*)&qopen.bus.dev,
			(unsigned int*)&qopen.bus.func,
			&qopen.bus.i2cbus);
		
		if (msgsnd(msqid, &qopen, QUERY_SIZE, IPC_NOWAIT) < 0) {
			perror(_("Error while sending open message"));
			return -3;
		}
		
		
		struct answer aopen;
		
		if (ddcpci_read(&aopen) < 0) {
			perror(_("Error while reading open message answer"));
			return -3;
		}
		
		mon->type = pci;
	}
#endif
#ifdef HAVE_AMDADL
	else if (strncmp(filename, "adl:", 4) == 0) {
		mon->adl_adapter = -1;
		mon->adl_display = -1;
		if (sscanf(filename, "adl:%d:%d", &mon->adl_adapter, &mon->adl_display) != 2){
			fprintf(stderr, _("Invalid filename (%s).\n"), filename);
			return -3;
		}

		if (amd_adl_check_display(mon->adl_adapter, mon->adl_display)){
			fprintf(stderr, _("ADL display not found (%s).\n"), filename);
			return -3;
		}

		mon->type = type_adl;
	}
#endif
	else {
		fprintf(stderr, _("Invalid filename (%s).\n"), filename);
		return -3;
	}
	
	mon->addr = addr;
	
	if (ddcci_read_edid(mon, edid) < 0) {
		return -2;
	}
	
	ddcci_caps(mon);
	mon->db = ddcci_create_db(mon->pnpid, &mon->caps, 1);
	mon->fallback = 0; /* No fallback */
	
	if (!mon->db) {
		/* Fallback on manufacturer generic profile */
		char buffer[7];
		buffer[0] = 0;
		strncat(buffer, mon->pnpid, 3); /* copy manufacturer id */
		switch(mon->caps.type) {
		case lcd:
			strcat(buffer, "lcd");
			mon->db = ddcci_create_db(buffer, &mon->caps, 1);
			mon->fallback = 1;
			break;
		case crt:
			strcat(buffer, "crt");
			mon->db = ddcci_create_db(buffer, &mon->caps, 1);
			mon->fallback = 1;
			break;
		case unk:
			break;
		}
		
		if (!mon->db) {
			/* Fallback on VESA generic profile */
			mon->db = ddcci_create_db("VESA", &mon->caps, 1);
			mon->fallback = 2;
		}
	}
	
	if ((mon->db) && (mon->db->init == samsung)) {
		if (ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_ENABLE, 0) < 0) {
			return -1;
		}
	}
	else {
		if (ddcci_command(mon, DDCCI_COMMAND_PRESENCE) < 0) {
			return -1;
		}
	}
	
	return 0;
}

int ddcci_open(struct monitor* mon, const char* filename, int probing) 
{
	return ddcci_open_with_addr(mon, filename, DEFAULT_DDCCI_ADDR, DEFAULT_EDID_ADDR, probing);
}

int ddcci_save(struct monitor* mon) 
{
	return ddcci_command(mon, DDCCI_COMMAND_SAVE);
}

/* Returns :
  - 0 if OK
  - -1 if DDC/CI is not available
  - -3 if file can't be closed 
*/
int ddcci_close(struct monitor* mon)
{
	// TODO: closing and freeing are different operations, split the function!

	if(mon->__vtable) {
		return mon->__vtable->close(mon);
	}

	if (mon->db)
	{
		if (mon->db->init == samsung) {
			if ((ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_DISABLE, 0)) < 0) {
				return -1;
			}
		}
		ddcci_free_db(mon->db);
	}
	else
	{ /* Alternate way of init mode detecting for unsupported monitors */
		if (strncmp(mon->pnpid, "SAM", 3) == 0) {
			if ((ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_DISABLE, 0)) < 0) {
				return -1;
			}
		}
	}
	
	int i;
	for (i = 0; i < 256; i++) {
		if(mon->caps.vcp[i]) {
			if (mon->caps.vcp[i]->values) {
				free(mon->caps.vcp[i]->values);
			}
			free(mon->caps.vcp[i]);
		}
	}

	free(mon->caps.raw_caps);
	
	if (mon->profiles) {
		ddcci_free_profile(mon->profiles);
	}
	
	if ((mon->fd > -1) && (close(mon->fd) < 0)) {
		return -3;
	}
	
	return 0;
}

void ddcci_probe_device(char* filename, struct monitorlist** current, struct monitorlist*** last) {
	struct monitor mon;
	int ret = ddcci_open(&mon, filename, 1);
	
	if (verbosity) {
		printf(_("ddcci_open returned %d\n"), ret);
	}
	
	if (ret > -2) { /* At least the EDID has been read correctly */
		(*current) = malloc(sizeof(struct monitorlist));
		(*current)->filename = filename;
		(*current)->supported = (ret == 0);
		if (mon.db) {
			(*current)->name = malloc(strlen((char*)mon.db->name)+1);
			strcpy((char*)(*current)->name, (char*)mon.db->name);
		}
		else {
			(*current)->name = malloc(32);
			snprintf((char*)(*current)->name, 32, _("Unknown monitor (%s)"), mon.pnpid);
		}
		(*current)->digital = mon.digital;
		(*current)->next = NULL;
		**last = (*current);
		*last = &(*current)->next;
	}
	else {
		free(filename);
	}
	
	ddcci_close(&mon);
}

struct monitorlist* ddcci_probe() {
	char* filename = NULL;
	
	struct monitorlist* list = NULL;
	struct monitorlist* current = NULL;
	struct monitorlist** last = &list;
	
	printf(_("Probing for available monitors"));
	if (verbosity)
		printf("...\n");
	fflush(stdout);
	
#ifdef HAVE_DDCPCI
	/* Fetch bus list from ddcpci */
	if (msqid >= 0) {
		struct query qlist;
		memset(&qlist, 0, sizeof(struct query));
		qlist.mtype = 1;
		qlist.qtype = QUERY_LIST;
		
		if (msgsnd(msqid, &qlist, QUERY_SIZE, IPC_NOWAIT) < 0) {
			perror(_("Error while sending list message"));
		}
		else {
			int len = 0, i;
			struct answer alist;
			char** filelist = NULL;
			
			while (1) {
				if (ddcpci_read(&alist) < 0){
					perror(_("Error while reading list entry"));
					break;
				}
				else {
					if (alist.last == 1) {
						break;
					}
					
					filelist = realloc(filelist, (len+1)*sizeof(struct answer));
					
					//printf("<==%02x:%02x.%d-%d\n", alist.bus.bus, alist.bus.dev, alist.bus.func, alist.bus.i2cbus);
					filelist[len] = malloc(32);
					
					snprintf(filelist[len], 32, "pci:%02x:%02x.%d-%d", 
						alist.bus.bus, alist.bus.dev, alist.bus.func, alist.bus.i2cbus);
					
					if (verbosity) {
						printf(_("Found PCI device (%s)\n"), filelist[len]);
					}
					
					len++;
				}
			}
			
			for (i = 0; i < len; i++) {
				ddcci_probe_device(filelist[i], &current, &last);
				if (!verbosity) {
					printf(".");
					fflush(stdout);
				}
			}
			free(filelist);
		}
	}
#endif
	
	/* Probe real I2C device */
	DIR *dirp;
	struct dirent *direntp;
	
	dirp = opendir("/dev/");
	
#ifdef __FreeBSD__
	const char *prefix = "iic";
#else
	const char *prefix = "i2c-";
#endif
	int prefix_len = strlen(prefix);
	while ((direntp = readdir(dirp)) != NULL)
	{
		if (!strncmp(direntp->d_name, prefix, prefix_len))
		{
			filename = malloc(strlen(direntp->d_name)+12);
			
			snprintf(filename, strlen(direntp->d_name)+12, "dev:/dev/%s", direntp->d_name);
			
			if (verbosity) {
				printf(_("Found I2C device (%s)\n"), filename);
			}
			
			ddcci_probe_device(filename, &current, &last);
			if (!verbosity) {
				printf(".");
				fflush(stdout);
			}
		}
	}
	
	closedir(dirp);
	
#ifdef HAVE_AMDADL
	/* ADL probe */
	int adl_disp;

	for (adl_disp=0; adl_disp<amd_adl_get_displays_count(); adl_disp++){
		int adapter, display;
		if (amd_adl_get_display(adl_disp, &adapter, &display))
		    break;

			filename = malloc(64);
			snprintf(filename, 64, "adl:%d:%d", adapter, display);
			if (verbosity) {
				printf(_("Found ADL display (%s)\n"), filename);
			}
			ddcci_probe_device(filename, &current, &last);
			if (!verbosity) {
				printf(".");
				fflush(stdout);
		}
	}
#endif

	if (!verbosity)
		printf("\n");
	
	return list;
}

void ddcci_free_list(struct monitorlist* list) {
	if (list == NULL) {
		return;
	}
	free(list->filename);
	free(list->name);
	ddcci_free_list(list->next);
	free(list);
}

/* Create $HOME/.ddccontrol and subdirectories if necessary */
int ddcci_create_config_dir()
{
	int len, ret;
	char* home;
	char* filename;
	int trailing;
	struct stat buf;
	
	home     = getenv("HOME");
	trailing = (home[strlen(home)-1] == '/');
	
	len = strlen(home) + 32;
	
	filename = malloc(len);
	ret = snprintf(filename, len, "%s%s.ddccontrol", home, trailing ? "" : "/");
	DDCCI_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {free(filename);})
	
	if (stat(filename, &buf) < 0) {
		if (errno != ENOENT) {
			perror(_("Error while getting information about ddccontrol home directory."));
			free(filename);
			return 0;
		}
		
		if (mkdir(filename, 0750) < 0) {
			perror(_("Error while creating ddccontrol home directory."));
			free(filename);
			return 0;
		}
		
		if (stat(filename, &buf) < 0) {
			perror(_("Error while getting information about ddccontrol home directory after creating it."));
			free(filename);
			return 0;
		}
	}
	
	if (!S_ISDIR(buf.st_mode)) {
		errno = ENOTDIR;
		perror(_("Error: '.ddccontrol' in your home directory is not a directory."));
		free(filename);
		return 0;
	}
	
	strcat(filename, "/profiles");
	
	if (stat(filename, &buf) < 0) {
		if (errno != ENOENT) {
			perror(_("Error while getting information about ddccontrol profile directory."));
			free(filename);
			return 0;
		}
		
		if (mkdir(filename, 0750) < 0) {
			perror(_("Error while creating ddccontrol profile directory."));
			free(filename);
			return 0;
		}
		
		if (stat(filename, &buf) < 0) {
			perror(_("Error while getting information about ddccontrol profile directory after creating it."));
			free(filename);
			return 0;
		}
	}
	
	if (!S_ISDIR(buf.st_mode)) {
		errno = ENOTDIR;
		perror(_("Error: '.ddccontrol/profiles' in your home directory is not a directory."));
		free(filename);
		return 0;
	}
	
	free(filename);
	
	return 1;
}
