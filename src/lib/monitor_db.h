/*
    ddc/ci interface functions header
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
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

#ifndef MONITOR_DB_H
#define MONITOR_DB_H

#include "ddcci.h"

#include <libxml/xmlstring.h>

/* Current database version */
#define DBVERSION 3

enum control_type {
	value = 0,
	command = 1,
	list = 2,
	CONTROL_TYPE_VALUE = value,
	CONTROL_TYPE_COMMAND = command,
	CONTROL_TYPE_LIST = list
};
typedef enum control_type ControlType;

enum refresh_type {
	none = 0,
	all = 1,
	REFRESH_TYPE_NONE = none,
	REFRESH_TYPE_ALL = all
};
typedef enum refresh_type RefreshType;

enum init_type {
	unknown = 0,
	standard = 1,
	samsung = 2,
	INIT_TYPE_UNKNOWN = unknown,
	INIT_TYPE_STANDARD = standard,
	INIT_TYPE_SAMSUNG = samsung
};
typedef enum init_type InitType;

struct value_db {
	xmlChar* id;
	xmlChar* name;
	unsigned char value;
	
	struct value_db* next;
};
typedef struct value_db ValueDB;

struct control_db {
	xmlChar* id;
	xmlChar* name;
	unsigned char address;
	int delay; /* -1 indicate default value */
	enum control_type type;
	enum refresh_type refresh;
	
	struct control_db* next;
	struct value_db* value_list;
};
typedef struct control_db ControlDB;

struct subgroup_db {
	xmlChar* name;
	xmlChar* pattern;
	
	struct subgroup_db* next;
	struct control_db* control_list;
};
typedef struct subgroup_db SubgroupDB;

struct group_db {
	xmlChar* name;
	
	struct group_db* next;
	struct subgroup_db* subgroup_list;
};
typedef struct group_db GroupDB;

struct monitor_db {
	xmlChar* name;
	enum init_type init;
	
	struct group_db* group_list;
};
typedef struct monitor_db MonitorDB;

/* Load monitor profile data from the XML database. */
struct monitor_db* ddcci_create_db(const char* pnpname, struct caps* caps, int faulttolerance);
/* Free a monitor database returned by ddcci_create_db. */
void ddcci_free_db(struct monitor_db* mon_db);

/* Initialize monitor database subsystem. */
int ddcci_init_db(char* usedatadir);
/* Release monitor database subsystem resources. */
void ddcci_release_db();

#endif
