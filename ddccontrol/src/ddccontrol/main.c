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

#include "config.h"

#define RETRYS 3 /* number of retrys */

static void dumpctrl(struct monitor* mon, unsigned char ctrl, int force)
{
	unsigned short value, maximum;
	int retry, result;

	struct monitor_db* monitor = mon->db;
	struct group_db* group;
	struct control_db* control;
	struct value_db* valued;
	xmlChar* controlname = NULL;
	xmlChar* valuename = NULL;
	
	for (retry = RETRYS; retry; retry--) {
		if ((result = ddcci_readctrl(mon, ctrl, &value, &maximum)) >= 0) 
		{
			if ((result > 0) || force)
			{
				if (monitor) {
					group = monitor->group_list;
					while ((group != NULL) && (controlname == NULL)) {
						control = group->control_list;
						while ((control != NULL) && (controlname == NULL)) {
							valued = control->value_list;
							
							while ((valued != NULL) && (valuename == NULL)) {
								if (valued->value == value) {
									valuename = valued->name;
									break;
								}
								valued = valued->next;
							}
							
							if (control->address == ctrl) {
								controlname = control->name;
								break;
							}
							
							control = control->next;
						}
						group = group->next;
					}
				}
				if (controlname == NULL) {
					fprintf(stdout, "Control 0x%02x: %c/%d/%d [???]\n", 
						ctrl, (result > 0) ? '+' : '-',  value, maximum);
				}
				else if (valuename == NULL) {
					fprintf(stdout, "Control 0x%02x: %c/%d/%d [%s]\n", 
						ctrl, (result > 0) ? '+' : '-',  value, maximum, controlname);
				}
				else {
					fprintf(stdout, "Control 0x%02x: %c/%d/%d [%s - %s]\n",
						ctrl, (result > 0) ? '+' : '-',  value, maximum, controlname, valuename);
				}
			}
			break;
		}
	}
}

/* Show database for the monitor_db monitor.
 * If mon is not NULL, query control values.
 */
static void showdatabase(struct monitor_db* monitor, struct monitor* mon)
{
	struct group_db* group;
	struct control_db* control;
	struct value_db* valued;
	
	int retry;
	
	if (monitor)
	{
		printf("\n= %s\n", monitor->name);
		
		for (group = monitor->group_list; group != NULL; group = group->next)
		{
			printf("> %s\n", group->name);
			
			for (control = group->control_list; control != NULL; control = control->next)
			{
				printf("\t> id=%s, name=%s, address=%#x, delay=%dms, type=%d\n", 
					control->id, control->name, control->address, control->delay, control->type);
				
				valued = control->value_list;
				if (valued) {
					printf("\t  Possible values:\n");
				}
				
				for (; valued != NULL; valued = valued->next) {
					printf("\t\t> id=%s - name=%s, value=%d\n", valued->id, valued->name, valued->value);
				}
				
				for (retry = RETRYS; retry; retry--)
				{
					int result;
					unsigned short value, maximum;
					
					if (mon) {
						if ((result = ddcci_readctrl(mon, control->address, &value, &maximum)) >= 0) {
							printf("\t  %ssupported, value=%d, maximum=%d\n", 
								(result > 0) ? "" : "not ", value, maximum);
							break;
						}
					}
				}
				
				
			}
		}
	}
}

static void usage(char *name)
{
	fprintf(stderr,"Usage:\n");
	fprintf(stderr,"%s [-v] [-c] [-d] [-f] [-s] [-r ctrl] [-w value] dev\n", name);
	fprintf(stderr,"%s [-v] [-q] pnpid\n", name);
	fprintf(stderr,"\tdev: device, e.g. /dev/i2c-0\n");
	fprintf(stderr,"\t-c : query capability\n");
	fprintf(stderr,"\t-d : query ctrls 0 - 255\n");
	fprintf(stderr,"\t-r : query ctrl\n");
	fprintf(stderr,"\t-w : value to write to ctrl\n");
	fprintf(stderr,"\t-f : force (avoid validity checks)\n");
	fprintf(stderr,"\t-s : save settings\n");
	fprintf(stderr,"\t-v : verbosity (specify more to increase)\n");
	fprintf(stderr,"\tpnpid: pnpid, e.g. MEL4513\n");
	fprintf(stderr,"\t-q : query the database about controls supported by the monitor indicated by pnpid\n");
}

int main(int argc, char **argv)
{
	int i, retry, ret;
	
	/* filedescriptor and name of device */
	struct monitor mon;
	char *fn;
	
	/* what to do */
	int dump = 0;
	int ctrl = -1;
	int value = -1;
	int caps = 0;
	int save = 0;
	int force = 0;
	int verbosity = 0;
	int querydb = 0;
	
	fprintf(stdout, "ddccontrol version " VERSION "\n");
	fprintf(stdout, "Copyright 2004 Oleg I. Vdovikin (oleg@cs.msu.su)\n");
	fprintf(stdout, "Copyright 2004 Nicolas Boichat (nicolas@boichat.ch)\n");
	fprintf(stdout, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stdout, "You may redistribute copies of this program under the terms of the GNU General Public License.\n\n");
	
	while ((i=getopt(argc,argv,"hdr:w:csfvq")) >= 0)
	{
		switch(i) {
		case 'h':
			usage(argv[0]);
			exit(1);
			break;
		case 'r':
			if ((ctrl = strtol(optarg, NULL, 0)) < 0 || (ctrl > 255))
			{
				fprintf(stderr,"'%s' does not seem to be a valid register name\n", optarg);
				exit(1);
			}
			break;
		case 'w':
			if ((value = strtol(optarg, NULL, 0)) < 0 || (value > 65535))
			{
				fprintf(stderr,"'%s' does not seem to be a valid value.\n", optarg);
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
		case 'q':
			querydb++;
			break;
		}
	}
	
	if (optind == argc) 
	{
		usage(argv[0]);
		exit(1);
	}
	
	fn = argv[optind];
	
	ddcci_verbosity(verbosity);
	
	if (querydb) {
		struct monitor_db* monitor;
		
		fprintf(stdout, "Reading DB for pnpid %s...\n", fn);
		
		monitor = ddcci_create_db(fn);
		
		if (monitor) {
			showdatabase(monitor, NULL);
			
			ddcci_free_db(monitor);
		}
		else {
			fprintf(stdout, "Error while reading the DB.\n");
		}
		
		exit(0);
	}
	
	fprintf(stdout, "Reading EDID and initializing DDC/CI at bus %s...\n", fn);
	
	if ((ret = ddcci_open(&mon, fn)) < 0) {
		fprintf(stderr, "\nDDC/CI at %s is unusable (%d).\n", fn, ret);
	} else {
		fprintf(stdout, "\nEDID readings:\n");
		fprintf(stdout, "\tPlug and Play ID: %s [%s]\n", 
			mon.pnpid, mon.db ? mon.db->name : NULL);
		fprintf(stdout, "\tInput type: %s\n", mon.digital ? "Digital" : "Analog");

		if (caps) {
			unsigned char buf[1024];
			
			fprintf(stdout, "\nCapabilities:\n");
			
			for (retry = RETRYS; retry; retry--) {
				if (ddcci_caps(&mon, buf, 1024) >= 0) {
					fprintf(stdout, "%s\n", buf);
					break;
				}
			}
			
			if (retry == 0) {
				fprintf(stderr, "Capabilities read fail.\n");
			}
		}
		
		if (ctrl >= 0) {
			if (value >= 0) {
				fprintf(stdout, "\nWriting 0x%02x, 0x%02x(%d)...\n",
					ctrl, value, value);
				ddcci_writectrl(&mon, ctrl, value);
			} else {
				fprintf(stdout, "\nReading 0x%02x...\n", ctrl);
			}
			
			dumpctrl(&mon, ctrl, 1);
		}
		
		if (dump) {
			fprintf(stdout, "\nControls (valid/current/max) [Description - Value name]:\n");
			
			for (i = 0; i < 256; i++) {
				dumpctrl(&mon, i, force);
			}
		}
		else if (ctrl == -1 && caps == 0) {
			showdatabase(mon.db, &mon);
		}
		
		if (save) {
			fprintf(stdout, "\nSaving settings...\n");
	
			ddcci_save(&mon);
		}
	}
	
	ddcci_close(&mon);
	
	exit(0);
}
