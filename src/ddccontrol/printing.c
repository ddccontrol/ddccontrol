/*
    ddc/ci command line tool
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2006 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)

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

#include "ddccontrol.h"

#include "config.h"
#include "internal.h"
#include "monitor_db.h"

#include <stdio.h>


void print_control_value(struct monitor *mon, unsigned char ctrl, unsigned short value, unsigned short maximum, int result)
{
	struct monitor_db *monitor = mon != NULL ? mon->db : NULL;
	struct group_db *group;
	struct subgroup_db *subgroup;
	struct control_db *control;
	struct value_db *valued;
	xmlChar *controlname = NULL;
	xmlChar *valuename = NULL;

	if (monitor) {
		/* loop through groups */
		for (group = monitor->group_list; (group != NULL) &&
		     (controlname == NULL); group = group->next) {
			/* loop through subgroups inside group */
			for (subgroup = group->subgroup_list; (subgroup != NULL); subgroup = subgroup->next) {
				/* loop through controls inside subgroup */
				for (control = subgroup->control_list; (control != NULL); control = control->next) {
					/* check for control id */
					if (control->address == ctrl) {
						controlname = control->name;
						/* look for the value */
						for (valued = control->value_list; (valued != NULL); valued = valued->next) {
							if (valued->value == value) {
								valuename = valued->name;
								break;
							}
						}
						break;
					}
				}
			}
		}
	}
	if (controlname == NULL) {
		fprintf(stdout, "%s 0x%02x: %c/%d/%d %c [???]\n", _("Control"),
		        ctrl, (result > 0) ? '+' : '-', value, maximum, (mon != NULL && mon->caps.vcp[ctrl]) ? 'C' : ' ');
	} else if (valuename == NULL) {
		fprintf(stdout, "%s 0x%02x: %c/%d/%d %c [%s]\n", _("Control"),
		        ctrl, (result > 0) ? '+' : '-',  value, maximum, (mon != NULL && mon->caps.vcp[ctrl]) ? 'C' : ' ', controlname);
	} else {
		fprintf(stdout, "%s 0x%02x: %c/%d/%d %c [%s - %s]\n", _("Control"),
		        ctrl, (result > 0) ? '+' : '-',  value, maximum, (mon != NULL && mon->caps.vcp[ctrl]) ? 'C' : ' ', controlname, valuename);
	}
}

