/*
    ddc/ci interface functions
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2006 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

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
#include <strings.h>
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
#include "ddcci_protocol.h"
#include "internal.h"
#include "rust_ffi.h"

#include "conf.h"

extern int ddccontrol_caps_parse(const char *caps_str, struct caps *caps, int add);

/* ddc/ci defines */
#define DEFAULT_DDCCI_ADDR	0x37	/* ddc/ci logic sits at 0x37 */
#define DEFAULT_EDID_ADDR	0x50	/* edid sits at 0x50 */

#define DDCCI_COMMAND_READ	0x01	/* read ctrl value */
#define DDCCI_COMMAND_WRITE	0x03	/* write ctrl value */

#define DDCCI_COMMAND_SAVE	0x0c	/* save current settings */

#define DDCCI_COMMAND_CAPS	0xf3	/* get monitor caps */
#define DDCCI_COMMAND_PRESENCE	0xf7	/* ACCESS.bus presence check */

/* control numbers */
#define DDCCI_CTRL_BRIGHTNESS	0x10

/* samsung specific, magictune starts with writing 1 to this register */
#define DDCCI_CTRL		0xf5
#define DDCCI_CTRL_ENABLE	0x0001
#define DDCCI_CTRL_DISABLE	0x0000

/* ddc/ci iface tunables */
#define DELAY   		45000	/* uS to wait after write */

#define CONTROL_WRITE_DELAY   80000	/* uS to wait after writing to a control (default) */

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

int ddcci_init(char* usedatadir)
{
	if (!ddcci_init_db(usedatadir)) {
		printf(_("Failed to initialize ddccontrol database...\n"));
		return 0;
	}
	return 1;
}

void ddcci_release() {
	ddcci_release_db();
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
	default:
		return -1;
	}
}

/* read len bytes from i2c address addr into buf */
/* return len on success, -1 on failure */
static int i2c_read(struct monitor* mon, unsigned int addr, unsigned char *buf, unsigned char len)
{
	switch (mon->type) {
#ifdef HAVE_I2C_DEV
	case dev:
	{
		struct i2c_rdwr_ioctl_data msg_rdwr;
		struct i2c_msg             i2cmsg;
		int i, read_len;
	
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
	
		i = ioctl(mon->fd, I2C_RDWR, &msg_rdwr);
		read_len = ddcci_i2c_read_length(i, len);
		if (read_len < 0)
		{
			if (!mon->probing || verbosity) {
				perror("ioctl()");
				fprintf(stderr,_("ioctl returned %d\n"),i);
			}
			return -1;
		}

		if (verbosity > 1) {
			dumphex(stderr, "Recv", buf, read_len);
		}

		return read_len;
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
static int ddcci_write(struct monitor* mon, const unsigned char *buf, size_t len)
{
	unsigned char frame[DDCCI_MAX_FRAME_LEN];
	int frame_len = ddccontrol_ddcci_build_frame(mon->addr, buf, len,
		frame, sizeof(frame));
	if (frame_len < 0)
		return -1;

	/* wait for previous command to complete */
	ddcci_delay(mon, 1);

	return i2c_write(mon, mon->addr, frame, (unsigned char)frame_len);
}

/* read ddc/ci formatted frame from ddc/ci at address addr, to buf */
static int ddcci_read(struct monitor* mon, unsigned char *buf, size_t len)
{
	unsigned char frame[DDCCI_MAX_FRAME_LEN];
	int read_len, payload_len, missing_length_flag;
	int frame_len = ddccontrol_ddcci_frame_length(len);
	if (frame_len < 0 || !buf)
		return -1;

	/* wait for previous command to complete */
	ddcci_delay(mon, 0);

	read_len = i2c_read(mon, mon->addr, frame, (unsigned char)frame_len);
	if (read_len <= 0)
		return -1;
	payload_len = ddccontrol_ddcci_parse_frame(mon->addr, frame, (size_t)read_len,
		buf, len, &missing_length_flag);
	if (payload_len < 0) {
		if (!mon->probing || verbosity) {
			switch (payload_len) {
			case DDCCI_PROTOCOL_ADDRESS:
				fprintf(stderr, _("Invalid response, first byte is 0x%02x, should be 0x%02x\n"),
					frame[0], mon->addr * 2);
				break;
			case DDCCI_PROTOCOL_LENGTH:
				fprintf(stderr, _("Invalid response, truncated frame or invalid length.\n"));
				break;
			case DDCCI_PROTOCOL_CHECKSUM:
				fprintf(stderr, _("Invalid response, corrupted data.\n"));
				break;
			default:
				fprintf(stderr, _("Invalid DDC/CI response.\n"));
				break;
			}
			dumphex(stderr, NULL, frame, read_len);
		}
		return -1;
	}

	if (missing_length_flag && (!mon->probing || verbosity))
		fprintf(stderr, _("Non-fatal error: Invalid response, magic is 0x%02x\n"), frame[1]);
	return payload_len;
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
	unsigned char ctrl, unsigned char *buf, size_t len)
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
	
	if (len < 0)
		return -1;
	return ddccontrol_ddcci_parse_vcp(buf, (size_t)len, ctrl, value, maximum);
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
	return ddccontrol_caps_parse(caps_str, caps, add);
}

/* read capabilities raw data of ddc/ci at address addr starting at offset to buf */
static int ddcci_raw_caps(struct monitor* mon, unsigned int offset, unsigned char *buf, size_t len)
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

/* Mask binary payloads before passing the NUL-terminated text to Rust. The
 * received byte count, not strlen(), bounds payloads that may contain NULs. */
static int ddcci_normalize_caps(char *raw_caps, size_t length)
{
	size_t pos = 0;

	while (pos < length && raw_caps[pos] != '\0') {
		char *number, *endptr;
		long binary_len;
		size_t payload, remaining;

		if (length - pos < 4 || memcmp(raw_caps + pos, "bin(", 4) != 0) {
			pos++;
			continue;
		}

		number = raw_caps + pos + 4;
		errno = 0;
		binary_len = strtol(number, &endptr, 0);
		if (errno == ERANGE || endptr == number || binary_len < 0 || *endptr != '(')
			return -1;

		payload = (size_t)(endptr - raw_caps) + 1;
		remaining = length - payload;
		if ((unsigned long)binary_len > remaining)
			return -1;
		pos = payload + (size_t)binary_len;

		memset(raw_caps + payload, '#', (size_t)binary_len);
		/* Leave delimiter/whitespace validation to the existing CAPS parser. */
	}
	return 0;
}

int ddcci_caps(struct monitor* mon)
{
	if (mon->__vtable) {
		/* Backend-driven monitors must provide capabilities during open. */
		return mon->caps.raw_caps ? (int)strlen(mon->caps.raw_caps) : -1;
	}

	/* The terminating empty reply must still be addressable by a 16-bit offset. */
	const size_t max_caps_length = 0xffff;
	size_t bufferpos = 0;
	char *raw_caps, *resized;
	unsigned char buf[64];	/* 64 bytes chunk (was 35, but 173P+ send 43 bytes chunks) */
	int len, fragment_len;
	int retries = 3;

	free(mon->caps.raw_caps);
	mon->caps.raw_caps = NULL;
	raw_caps = malloc(1);
	if (!raw_caps)
		return -1;
	raw_caps[0] = '\0';

	for (;;) {
		if (retries == 0)
			goto fail;

		len = ddcci_raw_caps(mon, (unsigned int)bufferpos, buf, sizeof(buf));
		if (len < 0) {
			retries--;
			continue;
		}
		
		fragment_len = ddccontrol_ddcci_parse_caps_reply(buf, (size_t)len,
			(unsigned int)bufferpos);
		if (fragment_len < 0) {
			if (!mon->probing || verbosity) {
				fprintf(stderr, _("Invalid sequence in caps.\n"));
			}
			retries--;
			continue;
		}

		if (fragment_len == 0)
			break;
		if ((size_t)fragment_len > max_caps_length - bufferpos)
			goto fail;

		resized = realloc(raw_caps, bufferpos + (size_t)fragment_len + 1);
		if (!resized)
			goto fail;
		raw_caps = resized;
		memcpy(raw_caps + bufferpos, buf + 3, (size_t)fragment_len);
		bufferpos += (size_t)fragment_len;
		raw_caps[bufferpos] = '\0';
		retries = 3;
	}

	if (ddcci_normalize_caps(raw_caps, bufferpos) < 0) {
		if (!mon->probing || verbosity)
			fprintf(stderr, _("Invalid binary data in caps.\n"));
		goto fail;
	}
	if (ddcci_parse_caps(raw_caps, &mon->caps, 1) < 0)
		goto fail;

	mon->caps.raw_caps = raw_caps;
	return (int)bufferpos;

fail:
	free(raw_caps);
	return -1;
}

/* save current settings */
int ddcci_command(struct monitor* mon, unsigned char cmd)
{
	unsigned char _buf[1];

	_buf[0] = cmd;

	return ddcci_write(mon, _buf, sizeof(_buf));
}

/* Parse an EDID buffer and fill in mon->pnpid, mon->digital, mon->edid, and mon->edid_info.
 * Requires at least DDCCI_EDID_MIN_PARSE_LEN bytes and a valid EDID header.
 * Returns 0 on success, -1 on failure. */
int ddcci_parse_edid_buf(struct monitor* mon, const unsigned char* buf, int len)
{
	struct ddccontrol_edid_result parsed;

	if (!mon)
		return -1;

	mon->digital = 0;
	mon->edid_len = 0;
	memset(mon->edid, 0, sizeof(mon->edid));
	memset(&mon->edid_info, 0, sizeof(mon->edid_info));

	if (!buf || len < 0 || ddccontrol_edid_parse(buf, (size_t)len, &parsed) < 0)
		return -1;

	memcpy(mon->pnpid, parsed.pnpid, sizeof(mon->pnpid));
	mon->digital = parsed.digital;
	memcpy(mon->edid, parsed.edid, sizeof(mon->edid));
	mon->edid_len = parsed.edid_len;
	mon->edid_info = parsed.info;

	if (!mon->probing && verbosity) {
		printf(_("Serial number: %u\n"), mon->edid_info.serial_number);
		printf(_("Manufactured: Week %d, %d\n"),
		       mon->edid_info.manufacture_week, mon->edid_info.manufacture_year);
		printf(_("EDID version: %d.%d\n"), mon->edid_info.version, mon->edid_info.revision);
		printf(_("Maximum size: %d x %d (cm)\n"),
		       mon->edid_info.max_width_cm, mon->edid_info.max_height_cm);
		if (mon->edid_info.monitor_name[0])
			printf(_("Monitor name: %s\n"), mon->edid_info.monitor_name);
		if (mon->edid_info.serial_ascii[0])
			printf(_("Serial text: %s\n"), mon->edid_info.serial_ascii);
	}

	return 0;
}

int ddcci_read_edid(struct monitor* mon, int addr) 
{
	unsigned char buf[128];
	int retry;

	for (retry = 0; retry < 3; retry++) {
		buf[0] = 0;	/* eeprom offset */

		if (i2c_write(mon, addr, buf, 1) > 0 &&
		    i2c_read(mon, addr, buf, sizeof(buf)) > 0) 
		{		
			if (ddcci_parse_edid_buf(mon, buf, sizeof(buf)) == 0) {
				return 0;
			} else {
				if (retry == 2 && (!mon->probing || verbosity)) {
					fprintf(stderr, _("Corrupted EDID at 0x%02x.\n"), addr);
				}
			}
		} else if (retry == 2 && (!mon->probing || verbosity)) {
			fprintf(stderr, _("Reading EDID 0x%02x failed.\n"), addr);
		}

		usleep(DELAY);
	}

	return -1;
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
	int caps_result;

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
	else {
		fprintf(stderr, _("Invalid filename (%s).\n"), filename);
		return -3;
	}
	
	mon->addr = addr;
	
	if (ddcci_read_edid(mon, edid) < 0) {
		return -2;
	}
	
	caps_result = ddcci_caps(mon);
	mon->db = ddcci_create_db(mon->pnpid, &mon->caps, 1);
	mon->fallback = 0; /* No fallback */
	
	if (!mon->db) {
		/* Fallback on manufacturer generic profile */
		char buffer[8]; /* 3 chars (pnpid) + 3 chars (suffix) + 1 null terminator + 1 for safety */
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
	else if (!ddcci_caps_prove_support(caps_result)) {
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
	if (mon->__vtable) {
		return 0;
	}

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
			const char *dbname = (const char *)mon.db->name;
			if (mon.fallback > 0) {
				/* Include PnP ID so users can identify monitors not in the database */
				size_t len = strlen(dbname) + 3 + strlen(mon.pnpid) + 1;
				(*current)->name = malloc(len);
				snprintf((char *)(*current)->name, len, "%s [%s]", dbname, mon.pnpid);
			} else {
				(*current)->name = malloc(strlen(dbname) + 1);
				strcpy((char *)(*current)->name, dbname);
			}
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

static const char *ddcci_find_trailing_digits(const char *s) {
	const char *end = s + strlen(s);
	const char *p = end;

	while (p > s && p[-1] >= '0' && p[-1] <= '9') {
		p--;
	}

	if (p == end) {
		return NULL;
	}

	return p;
}

static int ddcci_cmp_dev_filenames(const void *a, const void *b) {
	const char *sa = *(const char *const *)a;
	const char *sb = *(const char *const *)b;
	const char *na = ddcci_find_trailing_digits(sa);
	const char *nb = ddcci_find_trailing_digits(sb);
	if (na && nb) {
		int ia = atoi(na);
		int ib = atoi(nb);
		if (ia != ib)
			return ia - ib;
	}
	return strcmp(sa, sb);
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
	
	/* Probe real I2C device */
	DIR *dirp;
	struct dirent *direntp;

#ifdef __FreeBSD__
	const char *prefix = "iic";
#else
	const char *prefix = "i2c-";
#endif
	int prefix_len = strlen(prefix);

	/* Collect matching device filenames first, then sort for consistent ordering */
	char **dev_filenames = NULL;
	int dev_count = 0;

	dirp = opendir("/dev/");
	while ((direntp = readdir(dirp)) != NULL)
	{
		if (!strncmp(direntp->d_name, prefix, prefix_len))
		{
			char **tmp_dev_filenames;

			filename = malloc(strlen(direntp->d_name) + 12);
			if (filename == NULL)
				break;
			snprintf(filename, strlen(direntp->d_name) + 12, "dev:/dev/%s", direntp->d_name);
			tmp_dev_filenames = realloc(dev_filenames, (dev_count + 1) * sizeof(char *));
			if (tmp_dev_filenames == NULL)
			{
				free(filename);
				break;
			}
			dev_filenames = tmp_dev_filenames;
			dev_filenames[dev_count++] = filename;
		}
	}
	
	closedir(dirp);

	/* Sort numerically by device number for predictable, consistent ordering */
	if (dev_count > 1)
		qsort(dev_filenames, dev_count, sizeof(char *), ddcci_cmp_dev_filenames);

	for (int i = 0; i < dev_count; i++)
	{
		filename = dev_filenames[i];
		if (verbosity) {
			printf(_("Found I2C device (%s)\n"), filename);
		}
		ddcci_probe_device(filename, &current, &last);
		if (!verbosity) {
			printf(".");
			fflush(stdout);
		}
	}
	free(dev_filenames);
	
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
	if ((home == NULL) || (home[0] == '\0')) {
		fprintf(stderr, _("Cannot get home directory (HOME is unset or empty)\n"));
		return 0;
	}
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
