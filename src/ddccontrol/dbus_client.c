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

#include "daemon/dbus_client.h"

#include "ddcci.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dumpctrl(DDCControl *proxy, char *fn, struct monitor* mon, unsigned char ctrl, int force)
{
	unsigned short value, maximum;
	int result = ddcci_readctrl(mon, ctrl, &value, &maximum);

	if ((result > 0) || force) {
		print_control_value(mon, ctrl, value, maximum, result);
	}
}

int perform_using_dbus(char *fn, int dump, int caps, int probe, int ctrl, int value, int force) {
	int i;

	// TODO: custom datadir
	if (!ddcci_init_db(NULL)) {
		printf(_("Unable to initialize ddcci database.\n"));
		exit(1);
	}

	DDCControl *proxy = ddcci_dbus_open_proxy();
	if(proxy == NULL)
		return -1;

	if (probe) {
		fn = NULL;

		struct monitorlist *monlist, *current;
		
		monlist = ddcci_dbus_rescan_monitors(proxy);
		current = monlist;
		while (current != NULL)
		{
			printf(_(" - Device: %s\n"), current->filename);
			printf(_("   DDC/CI supported: %s\n"), current->supported ? _("Yes") : _("No"));
			printf(_("   Monitor Name: %s\n"), current->name);
			printf(_("   Input type: %s\n"), current->digital ? _("Digital") : _("Analog"));
			
			if ((!fn) && (current->supported))
			{
				printf(_("   (Automatically selected)\n"));
				fn = malloc(strlen(current->filename)+1);
				strcpy(fn, current->filename);
			}
			
			current = current->next;
		}

		if (fn == NULL) {
			fprintf(stderr, _(
				"No monitor supporting DDC/CI available.\n"
				"If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n"
				));
			ddcci_release();
			exit(0);
		}
	}

	struct monitor *mon;
	ddcci_dbus_open(proxy, &mon, fn);

	if( ctrl >= 0 ) {
		if( value == -1 ) {
			fprintf(stdout, _("Reading 0x%02x...\n"), ctrl);
		} else {
			int result = ddcci_writectrl(mon, ctrl, value, 0 /* TODO */);
			if(result < 0) {
				printf(_("Write failed\n."));
			}
		}
		dumpctrl(proxy, fn, mon, ctrl, 1);
	}

	if (dump) {
		fprintf(stdout, _("\nControls (valid/current/max) [Description - Value name]:\n"));

		for (i = 0; i < 256; i++) {
			dumpctrl(proxy, fn, mon, i, force);
		}
	}
	return 0;
}

