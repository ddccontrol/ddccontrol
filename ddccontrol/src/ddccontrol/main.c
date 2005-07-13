/*
    ddc/ci command line tool
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

#include "ddcci.h"
#include "monitor_db.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define RETRYS 3 /* number of retrys */

static void dumpctrl(struct monitor* mon, unsigned char ctrl, int force)
{
	unsigned short value, maximum;
	int retry, result;

	struct monitor_db* monitor = mon->db;
	struct group_db* group;
	struct subgroup_db* subgroup;
	struct control_db* control;
	struct value_db* valued;
	xmlChar* controlname = NULL;
	xmlChar* valuename = NULL;
	
	for (retry = RETRYS; retry; retry--) {
		if ((result = ddcci_readctrl(mon, ctrl, &value, &maximum)) >= 0) 
		{
			if ((result > 0) || force)
			{
				if (monitor) 
				{
					/* loop through groups */
					for (group = monitor->group_list; (group != NULL) && 
							(controlname == NULL); group = group->next) 
					{
						/* loop through subgroups inside group */
						for (subgroup = group->subgroup_list; (subgroup != NULL); subgroup = subgroup->next) 
						{
							/* loop through controls inside subgroup */
							for (control = subgroup->control_list; (control != NULL); control = control->next) 
							{
								/* check for control id */
								if (control->address == ctrl) 
								{
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
					fprintf(stdout, "%s 0x%02x: %c/%d/%d [???]\n", _("Control"),
						ctrl, (result > 0) ? '+' : '-',  value, maximum);
				}
				else if (valuename == NULL) {
					fprintf(stdout, "%s 0x%02x: %c/%d/%d [%s]\n", _("Control"),
						ctrl, (result > 0) ? '+' : '-',  value, maximum, controlname);
				}
				else {
					fprintf(stdout, "%s 0x%02x: %c/%d/%d [%s - %s]\n", _("Control"),
						ctrl, (result > 0) ? '+' : '-',  value, maximum, controlname, valuename);
				}
			}
			break;
		}
	}
}

static void usage(char *name)
{
	fprintf(stderr,_(
		"Usage:\n"
		"%s [-b datadir] [-v] [-c] [-d] [-f] [-s] [-r ctrl [-w value]] [-p | dev]\n"
		"\tdev: device, e.g. dev:/dev/i2c-0\n"
		"\t-p : probe I2C devices to find monitor busses.\n"
		"\t-c : query capability\n"
		"\t-d : query ctrls 0 - 255\n"
		"\t-r : query ctrl\n"
		"\t-w : value to write to ctrl\n"
		"\t-f : force (avoid validity checks)\n"
		"\t-s : save settings\n"
		"\t-v : verbosity (specify more to increase)\n"
		"\t-b : ddccontrol-db directory (if other than " DATADIR ")\n"
	), name);
}

static void check_integrity(char* datadir, char* pnpname) {
	struct monitor_db* mon_db;
	
	printf("Checking options.xml integrity...\n");
	if (!ddcci_init_db(datadir)) {
		printf("[ FAILED ]\n");
		exit(1);
	}

	printf("[ OK ]\n");
	
	printf("Checking %s integrity...\n", pnpname);
	if (!(mon_db = ddcci_create_db(pnpname, ""))) {
		printf("[ FAILED ]\n");
		ddcci_release_db();
		exit(1);
	}
	
	printf("[ OK ]\n");
	
	ddcci_free_db(mon_db);
	
	ddcci_release_db();
	
	exit(0);
}

int main(int argc, char **argv)
{
	int i, retry, ret;
		
	/* filedescriptor and name of device */
	struct monitor mon;
	char *fn;
	
	char *datadir = NULL;
	char *pnpname = NULL; /* pnpname for -i parameter */
	
	/* what to do */
	int dump = 0;
	int ctrl = -1;
	int value = -1;
	int caps = 0;
	int save = 0;
	int force = 0;
	int verbosity = 0;
	int probe = 0;
	
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bindtextdomain("ddccontrol-db", LOCALEDIR);
	textdomain(PACKAGE);
	
	fprintf(stdout,
		_("ddccontrol version %s\n"
		"Copyright 2005 Oleg I. Vdovikin (oleg@cs.msu.su) and Nicolas Boichat (nicolas@boichat.ch)\n"
		"This program comes with ABSOLUTELY NO WARRANTY.\n"
		"You may redistribute copies of this program under the terms of the GNU General Public License.\n\n"),
		VERSION);
	
	while ((i=getopt(argc,argv,"hdr:w:csfvpb:i:")) >= 0)
	{
		switch(i) {
		case 'h':
			usage(argv[0]);
			exit(1);
			break;
		case 'b':
			datadir = optarg;
			break;
		case 'r':
			if ((ctrl = strtol(optarg, NULL, 0)) < 0 || (ctrl > 255))
			{
				fprintf(stderr,_("'%s' does not seem to be a valid register name\n"), optarg);
				exit(1);
			}
			break;
		case 'w':
			if (ctrl == -1) {
				fprintf(stderr,_("You cannot use -w parameter without -r.\n"));
				exit(1);
			}
			if ((value = strtol(optarg, NULL, 0)) < 0 || (value > 65535))
			{
				fprintf(stderr,_("'%s' does not seem to be a valid value.\n"), optarg);
				exit(1);
			}
			break;
		case 'c':
			caps++;
			break;
		case 'd':
			dump++;
			break;
		case 's':
			save++;
			break;
		case 'f':
			force++;
			break;
		case 'v':
			verbosity++;
			break;
		case 'p':
			probe++;
			break;
		case 'i': /* Undocumented developer parameter: check integrity of a specific EDID id */
			pnpname = optarg;
			break;
		}
	}
	
	ddcci_verbosity(verbosity);
	if (pnpname) {
		check_integrity(datadir, pnpname);
	}
	
	if ((optind == argc) && (!probe)) /* Nor device, nor probe option specified. */
	{
		usage(argv[0]);
		exit(1);
	}
	else if ((optind != argc) && (probe)) /* Device and probe option specified. */
	{
		usage(argv[0]);
		exit(1);
	}
	
	if (!ddcci_init(datadir)) {
		printf(_("Unable to initialize ddcci library.\n"));
		exit(1);
	}
	
	if (probe) {
		fn = NULL;
		
		struct monitorlist* monlist;
		struct monitorlist* current;
		
		monlist = ddcci_probe();
		
		printf(_("Detected monitors :\n"));
		
		current = monlist;
		while (current != NULL)
		{
			printf(_(" - Device: %s\n"), current->filename);
			printf(_("   DDC/CI supported: %s\n"), current->supported ? _("Yes") : _("No"));
			printf(_("   Monitor Name: %s\n"), current->name);
			printf(_("   Input type: %s\n"), current->digital ? _("Digital") : _("Analog"));
			
			if ((!fn) && (current->supported))
			{
				printf(_("  (Automatically selected)\n"));
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
		
		ddcci_free_list(monlist);
	}
	else {
		fn = argv[optind];
	}
	
	fprintf(stdout, _("Reading EDID and initializing DDC/CI at bus %s...\n"), fn);
	
	if ((ret = ddcci_open(&mon, fn)) < 0) {
		fprintf(stderr, _(
			"\nDDC/CI at %s is unusable (%d).\n"
			"If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n"
		    ), fn, ret);
	} else {
		fprintf(stdout, _("\nEDID readings:\n"));
		fprintf(stdout, _("\tPlug and Play ID: %s [%s]\n"), 
			mon.pnpid, mon.db ? mon.db->name : NULL);
		fprintf(stdout, _("\tInput type: %s\n"), mon.digital ? _("Digital") : _("Analog"));

		if (caps) {
			unsigned char buf[1024];
			
			fprintf(stdout, _("\nCapabilities:\n"));
			
			for (retry = RETRYS; retry; retry--) {
				if (ddcci_caps(&mon, buf, 1024) >= 0) {
					fprintf(stdout, "%s\n", buf);
					break;
				}
			}
			
			if (retry == 0) {
				fprintf(stderr, _("Capabilities read fail.\n"));
			}
		}
		
		if (ctrl >= 0) {
			if (value >= 0) {
				fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d)...\n"),
					ctrl, value, value);
				ddcci_writectrl(&mon, ctrl, value);
			} else {
				fprintf(stdout, _("\nReading 0x%02x...\n"), ctrl);
			}
			
			dumpctrl(&mon, ctrl, 1);
		}
		
		if (dump) {
			fprintf(stdout, _("\nControls (valid/current/max) [Description - Value name]:\n"));
			
			for (i = 0; i < 256; i++) {
				dumpctrl(&mon, i, force);
			}
		}
		else if (ctrl == -1 && caps == 0) 
		{
			struct monitor_db* monitor = mon.db;
			struct group_db* group;
			struct subgroup_db* subgroup;
			struct control_db* control;
			struct value_db* valued;
			
			if (monitor) {		
				printf("\n= %s\n", monitor->name);
				
				for (group = monitor->group_list; group != NULL; group = group->next) {
					printf("> %s\n", group->name);
					
					for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next) {
						printf("\t> %s\n", subgroup->name);
						
						for (control = subgroup->control_list; control != NULL; control = control->next) {
							printf(_("\t\t> id=%s, name=%s, address=%#x, delay=%dms, type=%d\n"), 
								control->id, control->name, control->address, control->delay, control->type);
							
							valued = control->value_list;
							if (valued) {
								printf(_("\t\t  Possible values:\n"));
							}
							
							for (; valued != NULL; valued = valued->next) {
								printf(_("\t\t\t> id=%s - name=%s, value=%d\n"), valued->id, valued->name, valued->value);
							}
							
							for (retry = RETRYS; retry; retry--) {
								int result;
								unsigned short value, maximum;
								
								if ((result = ddcci_readctrl(&mon, control->address, &value, &maximum)) >= 0) {
									printf(
										(result > 0) 
											? _("\t\t  supported, value=%d, maximum=%d\n")
											: _("\t\t  not supported, value=%d, maximum=%d\n"), value, maximum);
									break;
								}
							}
						}
					}
				}
			}
		}
		
		if (save) {
			fprintf(stdout, _("\nSaving settings...\n"));
	
			ddcci_save(&mon);
		}
	}
	
	ddcci_close(&mon);
	
	if (probe) {
		free(fn);
	}
	
	ddcci_release();
	exit(0);
}
