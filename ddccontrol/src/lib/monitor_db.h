/*
    ddc/ci interface functions header
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)

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

#ifndef MONITOR_DB_H
#define MONITOR_DB_H

#include <libxml/xmlstring.h>

enum control_type {
value = 0,
command = 1,
list = 2
};

enum init_type {
standard = 0,
samsung = 1
};

struct value_db {
	xmlChar* id;
	xmlChar* name;
	unsigned char value;
	
	struct value_db* next;
};

struct control_db {
	xmlChar* id;
	xmlChar* name;
	unsigned char address;
	int delay; /* -1 indicate default value */
	enum control_type type;
	
	struct control_db* next;
	struct value_db* value_list;
};

struct subgroup_db {
	xmlChar* name;
	
	struct subgroup_db* next;
	struct control_db* control_list;
};

struct group_db {
	xmlChar* name;
	
	struct group_db* next;
	struct subgroup_db* subgroup_list;
};

struct monitor_db {
	xmlChar* name;
	enum init_type init;
	
	struct group_db* group_list;
};

struct monitor_db* ddcci_create_db(const char* pnpname, const char* default_caps);
void ddcci_free_db(struct monitor_db* mon_db);

#endif
