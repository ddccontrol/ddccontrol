/*
    ddc/ci interface functions header
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2005 Nicolas Boichat (nicolas@boichat.ch)

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

#ifndef DDCCI_H
#define DDCCI_H

#include "config.h"

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#include <time.h>
#include <sys/time.h>

#include "monitor_db.h"

struct profile;

struct monitor {
	int fd;
	unsigned int addr;
	unsigned char pnpid[8];
	unsigned char digital; /* 0 - digital, 1 - analog */
	struct timeval last;
	struct monitor_db* db;
	
	struct profile* profiles; /* profiles available for this monitor. Filled by get_all_profiles. */
	
	enum {
		dev
#ifdef HAVE_DDCPCI
		,pci
#endif
	} type;
	int probing; /* are we probing? */
};

/* Struct used to return monitor data probed by ddcci_probe */
struct monitorlist {
	char* filename; /* I2C device filename */
	
	unsigned char supported; /* 0 - DDC/CI not supported, 1 - DDC/CI supported */
	char* name;
	unsigned char digital; /* 0 - digital, 1 - analog */
	
	struct monitorlist* next;
};

struct monitorlist* ddcci_probe();
void ddcci_free_list(struct monitorlist* list);

int ddcci_open(struct monitor* mon, const char* filename, int probing);
int ddcci_save(struct monitor* mon);
int ddcci_close(struct monitor* mon);

int ddcci_writectrl(struct monitor* mon, unsigned char ctrl, unsigned short value, int delay);

/* return values: < 0 - failure, 0 - contron not supported, > 0 - supported */
int ddcci_readctrl(struct monitor* mon, unsigned char ctrl, 
	unsigned short *value, unsigned short *maximum);

int ddcci_caps(struct monitor* mon, unsigned char *buffer, unsigned int buflen);

/* verbosity level (0 - normal, 1 - encoded data, 2 - ddc/ci frames) */
void ddcci_verbosity(int verbosity);

int get_verbosity();

int ddcci_init(char* usedatadir);

void ddcci_release();

void ddcpci_send_heartbeat();

/* Macros */
#define DDCCI_RETURN_IF_RUN(cond, value, message, node, run) \
	if (cond) { \
		if (node) \
			fprintf(stderr, "Error %s @%s:%d (%s:%ld)\n", message, __FILE__, __LINE__, \
				((xmlNodePtr)node)->doc->name, XML_GET_LINE(node)); \
		else \
			fprintf(stderr, "Error %s @%s:%d\n", message, __FILE__, __LINE__); \
		run \
		return value; \
	}

#define DDCCI_RETURN_IF(cond, value, message, node) \
	DDCCI_RETURN_IF_RUN(cond, value, message, node, {})

#endif //DDCCI_H
