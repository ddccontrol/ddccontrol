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

#include "config.h"

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "i2c-dev.h"
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>

#include "ddcci.h"

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
static int verbosity = 0;

void ddcci_verbosity(int _verbosity)
{
	verbosity = _verbosity;
}

/* IPC functions */
#ifdef HAVE_DDCPCI
#include "ddcpci-ipc.h"
#include <sys/msg.h>
#include <sys/ipc.h>

#define DDCPCI_RETRY_DELAY 10000 /* in us */
#define DDCPCI_RETRIES 100

/* debugging */
static void dumphex(FILE *f, unsigned char *buf, int len)
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

static int msqid = -2;

int ddcpci_init()
{
	if (msqid == -2) {
		if (verbosity) {
			printf("ddcpci initing...\n");
		}
		
		key_t key = ftok(BINDIR "/ddcpci", getpid());
		
		if ((msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
			perror(_("Error while initialisating the message queue"));
			return 0;
		}
		
		char buffer[256];
		
		snprintf(buffer, 256, BINDIR "/ddcpci %d %d &", verbosity, key);
		
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

int ddcci_init()
{
	if (!ddcci_init_db()) {
		printf(_("Failed to initialize ddccontrol database...\n"));
		return 0;
	}
	return ddcpci_init();
}

void ddcci_release() {
	ddcpci_release();
	ddcci_release_db();
}

/* write len bytes (stored in buf) to i2c address addr */
/* return 0 on success, -1 on failure */
static int i2c_write(struct monitor* mon, unsigned int addr, unsigned char *buf, unsigned char len)
{
	if (mon->type == dev) {
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
	
		if ((i = ioctl(mon->fd, I2C_RDWR, &msg_rdwr)) < 0 )
		{
			perror("ioctl()");
			fprintf(stderr,_("ioctl returned %d\n"),i);
			return -1;
		}
	
		return i;
	}
#ifdef HAVE_DDCPCI
	else if (mon->type == pci) {
		struct query qdata;
		qdata.mtype = 1;
		qdata.qtype = QUERY_DATA;
		qdata.addr = addr;
		qdata.flags = 0;
		memcpy(qdata.buffer, buf, len);
		
		if (msgsnd(msqid, &qdata, QUERY_SIZE + len, IPC_NOWAIT) < 0) {
			perror(_("Error while sending write message"));
			return -3;
		}
		
		struct answer adata;
		
		if (ddcpci_read(&adata) < 0) {
			perror(_("Error while reading write message answer"));
			return -1;
		}
		return adata.status;
	}
#endif
	else {
		return -1;
	}
}

/* read at most len bytes from i2c address addr, to buf */
/* return -1 on failure */
static int i2c_read(struct monitor* mon, unsigned int addr, unsigned char *buf, unsigned char len)
{
	if (mon->type == dev) {
		struct i2c_rdwr_ioctl_data msg_rdwr;
		struct i2c_msg             i2cmsg;
		int i;
	
		msg_rdwr.msgs = &i2cmsg;
		msg_rdwr.nmsgs = 1;
	
		i2cmsg.addr  = addr;
		i2cmsg.flags = I2C_M_RD;
		i2cmsg.len   = len;
		i2cmsg.buf   = buf;
	
		if ((i = ioctl(mon->fd, I2C_RDWR, &msg_rdwr)) < 0)
		{
			perror("ioctl()");
			fprintf(stderr,_("ioctl returned %d\n"),i);
			return -1;
		}
	
		return i;
	}
#ifdef HAVE_DDCPCI
	else if (mon->type == pci) {
		int ret;
		struct query qdata;
		qdata.mtype = 1;
		qdata.qtype = QUERY_DATA;
		qdata.addr = addr;
		qdata.flags = I2C_M_RD;
		qdata.len = len;
		
		if (msgsnd(msqid, &qdata, QUERY_SIZE, IPC_NOWAIT) < 0) {
			perror(_("Error while sending read message"));
			return -3;
		}
		
		struct answer adata;
		
		if ((ret = ddcpci_read(&adata)) < 0) {
			perror(_("Error while reading read message answer"));
			return -1;
		}
		
		memcpy(buf, adata.buffer, ret - ANSWER_SIZE);
		
		return ret - ANSWER_SIZE;
	}
#endif
	else {
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
		if (verbosity) {
			fprintf(stderr, _("Invalid response, first byte is 0x%02x, should be 0x%02x\n"),
				_buf[0], mon->addr * 2);
			dumphex(stderr, _buf, len + 3);
		}
		return -1;
	}

	if ((_buf[1] & MAGIC_2) == 0) {
		fprintf(stderr, _("Invalid response, magic is 0x%02x\n"), _buf[1]);
		return -1;
	}

	_len = _buf[1] & ~MAGIC_2;
	if (_len > len || _len > sizeof(_buf)) {
		fprintf(stderr, _("Invalid response, length is %d, should be %d at most\n"),
			_len, len);
		return -1;
	}

	/* get the xor value */
	for (i = 0; i < _len + 3; i++) {
		xor ^= _buf[i];
	}
	
	if (xor != 0) {
		fprintf(stderr, _("Invalid response, corrupted data - xor is 0x%02x, length 0x%02x\n"), xor, _len);
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

int ddcci_caps(struct monitor* mon, unsigned char *buffer, unsigned int buflen)
{
	int bufferpos = 0;
	unsigned char buf[35];	/* 35 bytes chunk */
	int offset = 0;
	int len, i;
	int retries = 3;
	
	do {
		buffer[bufferpos] = 0;
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
			fprintf(stderr, _("Invalid sequence in caps.\n"));
			retries--;
			continue;
		}

		for (i = 3; i < len; i++) {
			buffer[bufferpos++] = buf[i];
			if (bufferpos >= buflen) {
				fprintf(stderr, _("Buffer too small to contain caps.\n"));
				return -1;
			}
		}
		
		offset += len - 3;
		
		retries = 3;
	} while (len != 3);

	buffer[bufferpos] = 0;
	
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
		if (verbosity) {
			dumphex(stdout, buf, sizeof(buf));
		}
		
		if (buf[0] != 0 || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff ||
		    buf[4] != 0xff || buf[5] != 0xff || buf[6] != 0xff || buf[7] != 0)
		{
			fprintf(stderr, _("Corrupted EDID at 0x%02x.\n"), addr);
			return -1;
		}
		
		snprintf(mon->pnpid, 8, "%c%c%c%02X%02X", 
			((buf[8] >> 2) & 31) + 'A' - 1, 
			((buf[8] & 3) << 3) + (buf[9] >> 5) + 'A' - 1, 
			(buf[9] & 31) + 'A' - 1, buf[11], buf[10]);
		
		mon->digital = (buf[20] & 0x80);
		
		return 0;
	} 
	else {
		fprintf(stderr, _("Reading EDID 0x%02x failed.\n"), addr);
		return -1;
	}
}

/* Returns :
  - 0 if OK
  - -1 if DDC/CI is not available
  - -2 if EDID is not available
  - -3 if file can't be opened 
*/
static int ddcci_open_with_addr(struct monitor* mon, const char* filename, int addr, int edid) 
{
	memset(mon, 0, sizeof(struct monitor));
	
	if (strncmp(filename, "dev:", 4) == 0) {
		if ((mon->fd = open(filename+4, O_RDWR)) < 0) {
			perror(filename);
			//fprintf(stderr, _("Be sure you've modprobed i2c-dev and correct framebuffer device.\n"));
			return -3;
		}
		mon->type = dev;
	}
#ifdef HAVE_DDCPCI
	else if (strncmp(filename, "pci:", 4) == 0) {
		printf(_("Device: %s\n"), filename);
		
		struct query qopen;
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
	else {
		fprintf(stderr, _("Invalid filename (%s).\n"), filename);
		return -3;
	}
	
	mon->addr = addr;
	
	if (ddcci_read_edid(mon, edid) < 0) {
		return -2;
	}
	
	unsigned char buf[1024];
	
	if (ddcci_caps(mon, buf, 1024) > -1) {
		mon->db = ddcci_create_db(mon->pnpid, buf);
	}
	else {
		mon->db = ddcci_create_db(mon->pnpid, "");
	}
	
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

int ddcci_open(struct monitor* mon, const char* filename) 
{
	return ddcci_open_with_addr(mon, filename, DEFAULT_DDCCI_ADDR, DEFAULT_EDID_ADDR);
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
	if (mon->db)
	{
		if (mon->db->init == samsung) {
			if ((ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_DISABLE)) < 0) {
				return -1;
			}
		}
		ddcci_free_db(mon->db);
	}
	else
	{ /* Alternate way of init mode detecting for unsupported monitors */
		if (strncmp(mon->pnpid, "SAM", 3) == 0) {
			if ((ddcci_writectrl(mon, DDCCI_CTRL, DDCCI_CTRL_DISABLE)) < 0) {
				return -1;
			}
		}
	}
	
	if ((mon->fd > -1) && (close(mon->fd) < 0)) {
		return -3;
	}
	
	return 0;
}

void ddcci_probe_device(char* filename, struct monitorlist** current, struct monitorlist*** last) {
	struct monitor mon;
	int ret = ddcci_open(&mon, filename);
	
	if (verbosity) {
		printf(_("ddcci_open returned %d\n"), ret);
	}
	
	if (ret > -2) { /* At least the EDID has been read correctly */
		(*current) = malloc(sizeof(struct monitorlist));
		(*current)->filename = filename;
		(*current)->supported = (ret == 0);
		if (mon.db) {
			(*current)->name = malloc(strlen(mon.db->name)+1);
			strcpy((char*)(*current)->name, mon.db->name);
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
	
#ifdef HAVE_DDCPCI
	/* Fetch bus list from ddcpci */
	if (msqid >= 0) {
		struct query qlist;
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
					perror("Error while reading list entry");
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
						printf(_("Found I2C device (%s)\n"), filelist[len]);
					}
					
					len++;
				}
			}
			
			for (i = 0; i < len; i++) {
				ddcci_probe_device(filelist[i], &current, &last);
			}
			free(filelist);
		}
	}
#endif
	
	/* Probe real I2C device */
	DIR *dirp;
	struct dirent *direntp;
	
	dirp = opendir("/dev/");
	
	while ((direntp = readdir(dirp)) != NULL)
	{
		if (!strncmp(direntp->d_name, "i2c-", 4))
		{
			filename = malloc(strlen(direntp->d_name)+12);
			
			snprintf(filename, strlen(direntp->d_name)+12, "dev:/dev/%s", direntp->d_name);
			
			if (verbosity) {
				printf(_("Found I2C device (%s)\n"), filename);
			}
			
			ddcci_probe_device(filename, &current, &last);
		}
	}
	
	closedir(dirp);
	
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
