/*
    read/write configuration files in ~/.ddccontrol (profiles, cached monitor list)
    Copyright(c) 2005 Nicolas Boichat (nicolas@boichat.ch)

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

#ifndef PROFILE_H
#define PROFILE_H

#include "ddcci.h"

/* Read/write monitor list */

struct monitorlist* ddcci_load_list();
int ddcci_save_list(struct monitorlist* monlist);

/* Profile structures and functions */

/* Current profile format version */
#define PROFILEVERSION 1

struct profile {
	char* filename;
	
	xmlChar* name;
	xmlChar* pnpid;
	
	int size; /* Number of controls */
	char address[256];
	short value[256];
	
	struct profile* next; /* Next profile in the list (used by get_all_profiles) */
};

struct profile* ddcci_create_profile(struct monitor* mon, const char* address, int size);
int ddcci_apply_profile(struct profile* profile, struct monitor* mon);

void ddcci_set_profile_name(struct profile* profile, const char* name);

int ddcci_get_all_profiles(struct monitor* mon);

struct profile* ddcci_load_profile(const char* filename);
int ddcci_save_profile(struct profile* profile, struct monitor* monitor);

void ddcci_delete_profile(struct profile* profile, struct monitor* monitor);

void ddcci_free_profile(struct profile* profile);

#endif //PROFILE_H
