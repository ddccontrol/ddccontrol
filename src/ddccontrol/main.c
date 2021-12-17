/*
    ddc/ci command line tool
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2006 Nicolas Boichat (nicolas@boichat.ch)

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
#include "daemon/dbus_client.h"
#include "ddcci.h"
#include "internal.h"
#include "monitor_db.h"
#include "conf.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define RETRYS 3 /* number of retrys */

static void dumpctrl(struct monitor *mon, unsigned char ctrl, int force)
{
	unsigned short value, maximum;
	int retry, result;

	for (retry = RETRYS; retry; retry--) {
		if ((result = ddcci_readctrl(mon, ctrl, &value, &maximum)) >= 0) {
			if ((result > 0) || force) {
				print_control_value(mon, ctrl, value, maximum, result);
			}
			break;
		}
	}
}

/* Find the delay we must respect after writing to an address in the database. */
static int find_write_delay(struct monitor *mon, char ctrl)
{
	struct monitor_db *monitor = mon->db;
	struct group_db *group;
	struct subgroup_db *subgroup;
	struct control_db *control;

	if (monitor) {
		/* loop through groups */
		for (group = monitor->group_list; group != NULL; group = group->next) {
			/* loop through subgroups inside group */
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next) {
				/* loop through controls inside subgroup */
				for (control = subgroup->control_list; control != NULL; control = control->next) {
					/* check for control id */
					if (control->address == ctrl) {
						return control->delay;
					}
				}
			}
		}
	}
	return -1;
}

static void usage(char *name)
{
	fprintf(stderr, _(
	            "Usage:\n"
	            "%s [-b datadir] [-v] [-c] [-d] [-f] [-s] [-r ctrl [-w value]] [-l (profile path)] [-p | dev]\n"
	            "\tdev: device, e.g. dev:/dev/i2c-0\n"
	            "\t-p : probe I2C devices to find monitor buses\n"
	            "\t-c : query capability\n"
	            "\t-d : query ctrls 0 - 255\n"
	            "\t-r : query ctrl\n"
	            "\t-w : value to write to ctrl\n"
	            "\t-W : relatively change ctrl value (+/-)\n"
	            "\t-f : force (avoid validity checks)\n"
	            "\t-s : save settings\n"
	            "\t-v : verbosity (specify more to increase)\n"
	            "\t-b : ddccontrol-db directory (if other than %s)\n"
	            "\t-l : load values from XML profile file\n"
	        ), name, DATADIR);
}

static void check_integrity(char *datadir, char *pnpname)
{
	struct monitor_db *mon_db;

	printf(_("Checking %s integrity...\n"), "options.xml");
	if (!ddcci_init_db(datadir)) {
		printf(_("[ FAILED ]\n"));
		exit(1);
	}

	printf(_("[ OK ]\n"));

	/* Create caps with all controls. */
	char buf2[4];
	char buffer[256 * 3 + 25];
	strcpy(buffer, "(vcp(");
	int i;
	for (i = 0; i < 256; i++) {
		snprintf(buf2, 4, "%02x ", i);
		strcat(buffer, buf2);
	}
	strcat(buffer, "))");

	struct caps caps;
	ddcci_parse_caps(buffer, &caps, 1);

	printf(_("Checking %s integrity...\n"), pnpname);
	if (!(mon_db = ddcci_create_db(pnpname, &caps, 0))) {
		printf(_("[ FAILED ]\n"));
		ddcci_release_db();
		exit(1);
	}

	printf(_("[ OK ]\n"));

	ddcci_free_db(mon_db);

	ddcci_release_db();

	exit(0);
}

static int can_use_dbus_daemon()
{
	const char *disable_envvar = getenv("DDCCONTROL_NO_DAEMON");

	if (disable_envvar != NULL && strlen(disable_envvar) > 0)
		return 0;

	return 1;
}

int main(int argc, char **argv)
{
	int i, retry, ret;

	/* for dbus */
	DDCControl *proxy = NULL;

	/* filedescriptor and name of device */
	struct monitor *mon;
	char *fn;

	char *datadir = NULL;
	char *pnpname = NULL; /* pnpname for -i parameter */

	/* -l (load profile) parameter */
	struct profile *profilefile = NULL;

	/* what to do */
	int dump = 0;
	int ctrl = -1;
	int value = -1;
	int relative = 0;
	int caps = 0;
	int save = 0;
	int force = 0;
	int verbosity = 0;
	int probe = 0;

#ifdef HAVE_GETTEXT
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bindtextdomain("ddccontrol-db", LOCALEDIR);
	textdomain(PACKAGE);
#endif

	fprintf(stdout,
	        _("ddccontrol version %s\n"
	          "Copyright 2004-2005 Oleg I. Vdovikin (oleg@cs.msu.su)\n"
	          "Copyright 2004-2006 Nicolas Boichat (nicolas@boichat.ch)\n"
	          "This program comes with ABSOLUTELY NO WARRANTY.\n"
	          "You may redistribute copies of this program under the terms of the GNU General Public License.\n\n"), VERSION);

	while ((i = getopt(argc, argv, "hdr:w:W:csfvpb:i:l:")) >= 0) {
		switch (i) {
		case 'h':
			usage(argv[0]);
			exit(1);
			break;
		case 'b':
			datadir = optarg;
			break;
		case 'r':
			if ((ctrl = strtol(optarg, NULL, 0)) < 0 || (ctrl > 255)) {
				fprintf(stderr, _("'%s' does not seem to be a valid register name\n"), optarg);
				exit(1);
			}
			break;
		case 'w':
			if (ctrl == -1) {
				fprintf(stderr, _("You cannot use -w parameter without -r.\n"));
				exit(1);
			}
			if ((value = strtol(optarg, NULL, 0)) < 0 || (value > 65535)) {
				fprintf(stderr, _("'%s' does not seem to be a valid value.\n"), optarg);
				exit(1);
			}
			break;
		case 'W':
			if (ctrl == -1) {
				fprintf(stderr, _("You cannot use -W parameter without -r.\n"));
				exit(1);
			}
			if ((value = strtol(optarg, NULL, 0)) < -65535 || (value > 65535)) {
				fprintf(stderr, _("'%s' does not seem to be a valid value.\n"), optarg);
				exit(1);
			}
			relative = 1;
			break;
		case 'l':
			profilefile = ddcci_load_profile(optarg);
			if (!profilefile) {
				fprintf(stderr, "Failed to load profile: %s\n", optarg);
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

	if ((optind == argc) && (!probe)) { /* Nor device, nor probe option specified. */
		usage(argv[0]);
		exit(1);
	} else if ((optind != argc) && (probe)) { /* Device and probe option specified. */
		usage(argv[0]);
		exit(1);
	}

	if (!can_use_dbus_daemon()) {
		if (!ddcci_init(datadir)) {
			printf(_("Unable to initialize ddcci library.\n"));
			exit(1);
		}
	} else {
		// TODO: DB datadir should be same, as datadir of daemon, needs some integration
		if (!ddcci_init_db(datadir)) {
			printf(_("Unable to initialize ddcci db library.\n"));
			exit(1);
		}
		proxy = ddcci_dbus_open_proxy();
		if (proxy == NULL) {
			printf(_("Failed to open D-Bus proxy, try with DDCCONTROL_NO_DAEMON=1.\n"));
			exit(1);
		}
	}

	if (probe) {
		fn = NULL;

		struct monitorlist *monlist;
		struct monitorlist *current;

		if (can_use_dbus_daemon()) {
			monlist = ddcci_dbus_rescan_monitors(proxy);
		} else {
			monlist = ddcci_probe();
		}

		printf(_("Detected monitors :\n"));

		current = monlist;
		while (current != NULL) {
			printf(_(" - Device: %s\n"), current->filename);
			printf(_("   DDC/CI supported: %s\n"), current->supported ? _("Yes") : _("No"));
			printf(_("   Monitor Name: %s\n"), current->name);
			printf(_("   Input type: %s\n"), current->digital ? _("Digital") : _("Analog"));

			if ((!fn) && (current->supported)) {
				printf(_("  (Automatically selected)\n"));
				fn = malloc(strlen(current->filename) + 1);
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
	} else {
		fn = argv[optind];
	}

	fprintf(stdout, _("Reading EDID and initializing DDC/CI at bus %s...\n"), fn);

	if (can_use_dbus_daemon()) {
		ret = ddcci_dbus_open(proxy, &mon, fn);
	} else {
		mon = malloc(sizeof(struct monitor));
		ret = ddcci_open(mon, fn, 0);
	}

	if (ret < 0) {
		fprintf(stderr, _(
		            "\nDDC/CI at %s is unusable (%d).\n"
		            "If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n"
		        ), fn, ret);
	} else {
		fprintf(stdout, _("\nEDID readings:\n"));
		fprintf(stdout, _("\tPlug and Play ID: %s [%s]\n"),
		        mon->pnpid, mon->db ? mon->db->name : NULL);
		fprintf(stdout, _("\tInput type: %s\n"), mon->digital ? _("Digital") : _("Analog"));

		if (mon->fallback) {
			/* Put a big warning (in red if we are writing to a terminal). */
			printf("%s%s\n", isatty(1) ? "\x1B[0;31m" : "", _("=============================== WARNING ==============================="));
			if (mon->fallback == 1) {
				printf(_(
				           "There is no support for your monitor in the database, but ddccontrol is\n"
				           "using a generic profile for your monitor's manufacturer. Some controls\n"
				           "may not be supported, or may not work as expected.\n"));
			} else if (mon->fallback == 2) {
				printf(_(
				           "There is no support for your monitor in the database, but ddccontrol is\n"
				           "using a basic generic profile. Many controls will not be supported, and\n"
				           "some controls may not work as expected.\n"));
			}
			printf(_(
			           "Please update ddccontrol-db, or, if you are already using the latest\n"
			           "version, please send the output of the following command to\n"
			           "ddccontrol-users@lists.sourceforge.net:\n"));
			printf("\nLANG= LC_ALL= ddccontrol -p -c -d\n\n");
			printf(_("Thank you.\n"));
			printf("%s%s\n", _("=============================== WARNING ==============================="), isatty(1) ? "\x1B[0m" : "");
		}

		if (caps) {
			fprintf(stdout, _("\nCapabilities:\n"));

			for (retry = RETRYS; retry; retry--) {
				if (ddcci_caps(mon) >= 0) {
					fprintf(stdout, _("Raw output: %s\n"), mon->caps.raw_caps);

					fprintf(stdout, _("Parsed output: \n"));
					fprintf(stdout, "\tVCP: ");
					int i;
					for (i = 0; i < 256; i++) {
						if (mon->caps.vcp[i]) {
							printf("%02x ", i);
						}
					}
					printf("\n");
					printf(_("\tType: "));
					switch (mon->caps.type) {
					case lcd:
						printf(_("LCD"));
						break;
					case crt:
						printf(_("CRT"));
						break;
					case unk:
						printf(_("Unknown"));
						break;
					}
					printf("\n");
					break;
				}
			}

			if (retry == 0) {
				fprintf(stderr, _("Capabilities read fail.\n"));
			}
		}

		if (ctrl >= 0) {
			if (relative) {
				unsigned short old_value, maximum;
				int retry, result;

				for (retry = RETRYS; retry || (result = ddcci_readctrl(mon, ctrl, &old_value, &maximum)) < 0; retry--)
					;

				value += old_value;
				if (value < 0) {
					fprintf(stderr, _("Value cannot be lower than zero! %d -> %d\n"), old_value, value);
					relative = 0;
				} else if (value > maximum) {
					fprintf(stderr, _("Value cannot be higher than maximum! %d / %d\n"), value, maximum);
					relative = 0;
					value = -1;
				}
			}
			if (value >= 0 || relative) {
				int delay = find_write_delay(mon, ctrl);
				if (delay >= 0) {
					fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d) (%dms delay)...\n"),
					        ctrl, value, value, delay);
				} else {
					fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d)...\n"),
					        ctrl, value, value);
				}
				ddcci_writectrl(mon, ctrl, value, delay);
			} else if (!relative) {
				fprintf(stdout, _("\nReading 0x%02x...\n"), ctrl);
			}

			dumpctrl(mon, ctrl, 1);
		}

		if (profilefile) {
			int result = ddcci_apply_profile(profilefile, mon);
			if (result) {
				fprintf(stdout, _("Applied profile: %s\n"), profilefile->name);
			} else {
				fprintf(stderr, _("Failed to apply profile: %s\n"), profilefile->name);
			}
			free(profilefile);
		} else if (dump) {
			fprintf(stdout, _("\nControls (valid/current/max) [Description - Value name]:\n"));

			for (i = 0; i < 256; i++) {
				dumpctrl(mon, i, force);
			}
		} else if (ctrl == -1 && caps == 0) {
			struct monitor_db *monitor = mon->db;
			struct group_db *group;
			struct subgroup_db *subgroup;
			struct control_db *control;
			struct value_db *valued;

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

								if ((result = ddcci_readctrl(mon, control->address, &value, &maximum)) >= 0) {
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

			ddcci_save(mon);
		}
	}

	ddcci_close(mon);
	free(mon);

	if (probe) {
		free(fn);
	}

	ddcci_release();
	exit(0);
}
