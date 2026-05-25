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
#include "monitor_db_internal.h"
#include "conf.h"
#include "issueurl.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

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
	            "%s [-b datadir] [-v] [-c] [-d] [-f] [-s] [-S] [-r ctrl [-w value|-W value|-t value1,value2]] [-l (profile path)] [-p | dev]\n"
	            "\tdev: device, e.g. dev:/dev/i2c-0, or monitor selector (name/model[/index])\n"
	            "\t-p : probe I2C devices to find monitor buses\n"
	            "\t-c : query capability\n"
	            "\t-d : query ctrls 0 - 255\n"
	            "\t-r : query ctrl\n"
	            "\t-w : value to write to ctrl\n"
	            "\t-W : relatively change ctrl value (+/-)\n"
	            "\t-t : toggle ctrl value between value1 and value2\n"
	            "\t-f : force (avoid validity checks)\n"
	            "\t-s : ask the monitor to save current settings (if supported)\n"
	            "\t-S : suppress unsupported-monitor fallback warning\n"
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
	char buffer[sizeof("(vcp(") + (3 * 256) + sizeof("))")];
	int pos = snprintf(buffer, sizeof(buffer), "(vcp(");
	if (pos < 0 || (size_t)pos >= sizeof(buffer)) {
		fprintf(stderr, "Failed to build monitor capability buffer.\n");
		ddcci_release_db();
		exit(1);
	}
	int i;
	for (i = 0; i < 256; i++) {
		int n = snprintf(buffer + pos, sizeof(buffer) - (size_t)pos, "%02x ", i);
		if (n < 0 || (size_t)n >= sizeof(buffer) - (size_t)pos) {
			fprintf(stderr, "Failed to build monitor capability buffer.\n");
			ddcci_release_db();
			exit(1);
		}
		pos += n;
	}
	{
		int n = snprintf(buffer + pos, sizeof(buffer) - (size_t)pos, "))");
		if (n < 0 || (size_t)n >= sizeof(buffer) - (size_t)pos) {
			fprintf(stderr, "Failed to build monitor capability buffer.\n");
			ddcci_release_db();
			exit(1);
		}
	}

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

static int parse_selector_index(const char *selector, char **base_selector, int *has_index, int *index)
{
	const char *slash = strrchr(selector, '/');
	char *endptr = NULL;
	long parsed_index;

	*has_index = 0;
	*index = -1;
	*base_selector = strdup(selector);
	if (*base_selector == NULL) {
		return 0;
	}

	if (slash == NULL || slash == selector || *(slash + 1) == '\0') {
		return 1;
	}

	for (const char *p = slash + 1; *p != '\0'; p++) {
		if (!isdigit((unsigned char)*p)) {
			return 1;
		}
	}

	parsed_index = strtol(slash + 1, &endptr, 10);
	if (*endptr != '\0' || parsed_index < 0) {
		return 1;
	}

	char *parsed_selector = strndup(selector, (size_t)(slash - selector));
	if (parsed_selector == NULL) {
		return 0;
	}
	free(*base_selector);
	*base_selector = parsed_selector;
	if (*base_selector == NULL || (*base_selector)[0] == '\0') {
		free(*base_selector);
		*base_selector = NULL;
		return 0;
	}
	*has_index = 1;
	*index = (int)parsed_index;
	return 1;
}

static int monitor_matches_selector(const char *selector, const struct monitorlist *monitor_entry, const struct monitor *mon)
{
	if (monitor_entry->name && strcmp(selector, monitor_entry->name) == 0) {
		return 1;
	}
	if (strcmp(selector, mon->pnpid) == 0) {
		return 1;
	}
	if (mon->db && mon->db->name && strcmp(selector, (const char *)mon->db->name) == 0) {
		return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int i, retry, ret;

	/* for dbus */
	DDCControl *proxy = NULL;

	/* filedescriptor and name of device */
	struct monitor *mon;
	char *fn;
	char **selected_fns = NULL;
	char **selected_names = NULL;
	int selected_count = 0;
	int selected_alloc = 0;
	int selected_need_free = 0;

	char *datadir = NULL;
	char *pnpname = NULL; /* pnpname for -i parameter */
	char *selected_monitor_name = NULL;

	/* -l (load profile) parameter */
	struct profile *profilefile = NULL;

	struct issue_report report = {0};
	report.ddccontrol_version = VERSION;

	/* what to do */
	int dump = 0;
	int ctrl = -1;
	int value = -1;
	int relative = 0;
	int toggle = 0;
	int toggle_value1 = -1;
	int toggle_value2 = -1;
	int requested_value;
	int requested_relative;
	int requested_toggle;
	int caps = 0;
	int save = 0;
	int force = 0;
	int suppress_warning = 0;
	int verbosity = 0;
	int probe = 0;
	int caps_read_failed = 0;

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

	while ((i = getopt(argc, argv, "hdr:w:W:t:csfvpb:i:l:S")) >= 0) {
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
			if (relative || toggle) {
				fprintf(stderr, _("You cannot use -w parameter with -W or -t.\n"));
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
			if (value >= 0 || toggle) {
				fprintf(stderr, _("You cannot use -W parameter with -w or -t.\n"));
				exit(1);
			}
			if ((value = strtol(optarg, NULL, 0)) < -65535 || (value > 65535)) {
				fprintf(stderr, _("'%s' does not seem to be a valid value.\n"), optarg);
				exit(1);
			}
			relative = 1;
			break;
		case 't':
			if (ctrl == -1) {
				fprintf(stderr, _("You cannot use -t parameter without -r.\n"));
				exit(1);
			}
			if (value >= 0 || relative) {
				fprintf(stderr, _("You cannot use -t parameter with -w or -W.\n"));
				exit(1);
			}
			{
				char *toggle_pair = strdup(optarg);
				char *separator = toggle_pair ? strchr(toggle_pair, ',') : NULL;
				char *endptr = NULL;

				if (!toggle_pair || !separator || separator == toggle_pair || *(separator + 1) == '\0') {
					free(toggle_pair);
					fprintf(stderr, _("'%s' does not seem to be a valid toggle pair. Use value1,value2.\n"), optarg);
					exit(1);
				}

				*separator = '\0';
				toggle_value1 = strtol(toggle_pair, &endptr, 0);
				if (*endptr != '\0' || toggle_value1 < 0 || toggle_value1 > 65535) {
					fprintf(stderr, _("'%s' does not seem to be a valid value.\n"), toggle_pair);
					free(toggle_pair);
					exit(1);
				}
				toggle_value2 = strtol(separator + 1, &endptr, 0);
				if (*endptr != '\0' || toggle_value2 < 0 || toggle_value2 > 65535) {
					fprintf(stderr, _("'%s' does not seem to be a valid value.\n"), separator + 1);
					free(toggle_pair);
					exit(1);
				}

				free(toggle_pair);
				toggle = 1;
			}
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
		case 'S':
			suppress_warning++;
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
				fn = strdup(current->filename);
				selected_monitor_name = strdup(current->name);
				if (!fn || !selected_monitor_name) {
					fprintf(stderr, _("Memory allocation failed\n"));
					free(fn);
					free(selected_monitor_name);
					ddcci_free_list(monlist);
					ddcci_release();
					exit(1);
				}
				report.monitor_name = selected_monitor_name;
			}
			current = current->next;
		}


		if (fn == NULL) {
			fprintf(stderr, _(
			            "No monitor supporting DDC/CI available.\n"
			            "If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n"
			            "On many laptops, the internal eDP/LVDS panel does not expose DDC/CI, so only external monitors may work.\n"
			            "For support, please include output from:\n"
			            "LANG=C LC_ALL=C ddccontrol -p -c -d\n"
			        ));
			ddcci_release();
			exit(0);
		}

		ddcci_free_list(monlist);
	} else {
		const char *requested_target = argv[optind];
		int use_selector = strncmp(requested_target, "dev:", 4) != 0
		                   && strncmp(requested_target, "pci:", 4) != 0
		                   && strncmp(requested_target, "adl:", 4) != 0;

		if (use_selector) {
			struct monitorlist *monlist;
			struct monitorlist *current;
			char *selector = NULL;
			int has_index = 0;
			int selected_index = -1;
			int matched_count = 0;

			if (!parse_selector_index(requested_target, &selector, &has_index, &selected_index)) {
				fprintf(stderr, _("Failed to parse monitor selector.\n"));
				ddcci_release();
				exit(1);
			}

			if (can_use_dbus_daemon()) {
				monlist = ddcci_dbus_rescan_monitors(proxy);
			} else {
				monlist = ddcci_probe();
			}
			current = monlist;
			while (current != NULL) {
				if (current->supported) {
					struct monitor *candidate;
					int open_ret;

					if (can_use_dbus_daemon()) {
						open_ret = ddcci_dbus_open(proxy, &candidate, current->filename);
					} else {
						candidate = malloc(sizeof(struct monitor));
						open_ret = ddcci_open(candidate, current->filename, 0);
					}
					if (open_ret >= 0 && monitor_matches_selector(selector, current, candidate)) {
						if (!has_index || (selected_index == matched_count)) {
							if (selected_count == selected_alloc) {
								int new_alloc = selected_alloc ? selected_alloc * 2 : 4;
								char **new_fns = realloc(selected_fns, sizeof(char *) * (size_t)new_alloc);
								char **new_names = realloc(selected_names, sizeof(char *) * (size_t)new_alloc);
								if (!new_fns || !new_names) {
									free(new_fns);
									free(new_names);
									fprintf(stderr, _("Memory allocation failed\n"));
									ddcci_close(candidate);
									free(candidate);
									ddcci_free_list(monlist);
									free(selector);
									ddcci_release();
									exit(1);
								}
								selected_fns = new_fns;
								selected_names = new_names;
								selected_alloc = new_alloc;
							}
							selected_fns[selected_count] = strdup(current->filename);
							selected_names[selected_count] = current->name ? strdup(current->name) : NULL;
							if (selected_fns[selected_count] == NULL
							    || (current->name && selected_names[selected_count] == NULL)) {
								fprintf(stderr, _("Memory allocation failed\n"));
								ddcci_close(candidate);
								free(candidate);
								ddcci_free_list(monlist);
								free(selector);
								ddcci_release();
								exit(1);
							}
							selected_count++;
							selected_need_free = 1;
							if (has_index) {
								ddcci_close(candidate);
								free(candidate);
								break;
							}
						}
						matched_count++;
					}
					if (open_ret >= 0) {
						ddcci_close(candidate);
						free(candidate);
					}
				}
				current = current->next;
			}
			if (selected_count == 0) {
				if (has_index) {
					fprintf(stderr, _("No monitor matched selector '%s'.\n"), requested_target);
				} else {
					fprintf(stderr, _("No monitor matched selector '%s'.\n"), requested_target);
				}
				ddcci_free_list(monlist);
				free(selector);
				ddcci_release();
				exit(1);
			}
			ddcci_free_list(monlist);
			free(selector);
		} else {
			selected_count = 1;
			selected_alloc = 1;
			selected_fns = malloc(sizeof(char *));
			selected_names = malloc(sizeof(char *));
			if (!selected_fns || !selected_names) {
				fprintf(stderr, _("Memory allocation failed\n"));
				ddcci_release();
				exit(1);
			}
			selected_fns[0] = (char *)requested_target;
			selected_names[0] = NULL;
		}
	}
	if (probe) {
		selected_count = 1;
		selected_alloc = 1;
		selected_fns = malloc(sizeof(char *));
		selected_names = malloc(sizeof(char *));
		if (!selected_fns || !selected_names) {
			fprintf(stderr, _("Memory allocation failed\n"));
			ddcci_release();
			exit(1);
		}
		selected_fns[0] = fn;
		selected_names[0] = selected_monitor_name;
	}

	requested_value = value;
	requested_relative = relative;
	requested_toggle = toggle;

	for (i = 0; i < selected_count; i++) {
		int current_relative = requested_relative;
		int current_toggle = requested_toggle;
		int current_value = requested_value;

		fn = selected_fns[i];
		report.device = fn;
		report.monitor_name = selected_names[i];

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
			            "If this is a laptop internal display, please note many eDP/LVDS panels do not support DDC/CI.\n"
			        ), fn, ret);
		} else {
			fprintf(stdout, _("\nEDID readings:\n"));
			fprintf(stdout, _("\tPlug and Play ID: %s [%s]\n"),
			        mon->pnpid, mon->db ? mon->db->name : NULL);
			fprintf(stdout, _("\tInput type: %s\n"), mon->digital ? _("Digital") : _("Analog"));
			report.pnp_id = mon->pnpid;
			if (!report.monitor_name && mon->db)
				report.monitor_name = (const char *)mon->db->name;

			if (caps) {
				fprintf(stdout, _("\nCapabilities:\n"));

				for (retry = RETRYS; retry; retry--) {
					if (ddcci_caps(mon) >= 0) {
						fprintf(stdout, _("Raw output: %s\n"), mon->caps.raw_caps);

						fprintf(stdout, _("Parsed output: \n"));
						fprintf(stdout, "\tVCP: ");
						int j;
						for (j = 0; j < 256; j++) {
							if (mon->caps.vcp[j]) {
								printf("%02x ", j);
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
				if (current_toggle) {
					unsigned short old_value, maximum;
					int result = -1;

					for (retry = RETRYS; retry; retry--) {
						result = ddcci_readctrl(mon, ctrl, &old_value, &maximum);
						if (result >= 0) {
							break;
						}
					}

					if (result < 0) {
						fprintf(stderr, _("Control read fail.\n"));
						current_value = -1;
					} else {
						current_value = (old_value == toggle_value1) ? toggle_value2 : toggle_value1;
						if (current_value > maximum) {
							fprintf(stderr, _("Value cannot be higher than maximum! %d / %d\n"), current_value, maximum);
							current_value = -1;
						}
					}
				}

				if (current_relative) {
					unsigned short old_value, maximum;
					int retry, result;

					for (retry = RETRYS; retry || (result = ddcci_readctrl(mon, ctrl, &old_value, &maximum)) < 0; retry--)
						;

					current_value += old_value;
					if (current_value < 0) {
						fprintf(stderr, _("Value cannot be lower than zero! %d -> %d\n"), old_value, current_value);
						current_relative = 0;
					} else if (current_value > maximum) {
						fprintf(stderr, _("Value cannot be higher than maximum! %d / %d\n"), current_value, maximum);
						current_relative = 0;
						current_value = -1;
					}
				}

				if (current_value >= 0 || current_relative) {
					int delay = find_write_delay(mon, ctrl);
					if (delay >= 0) {
						fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d) (%dms delay)...\n"),
						        ctrl, current_value, current_value, delay);
					} else {
						fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d)...\n"),
						        ctrl, current_value, current_value);
					}
					ddcci_writectrl(mon, ctrl, current_value, delay);
				} else if (!current_relative) {
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
			} else if (dump) {
				if (caps_read_failed) {
					fprintf(stderr, _("\nCapabilities query failed; continuing control scan.\n"));
				}
				fprintf(stdout, _("\nControls (valid/current/max) [Description - Value name]:\n"));

				for (int j = 0; j < 256; j++) {
					dumpctrl(mon, j, force);
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
									printf(_("\t\t\t> id=%s - name=%s, value=%u\n"), valued->id, valued->name,
									       (unsigned int)ddcci_value_db_value16(valued));
								}

								for (retry = RETRYS; retry; retry--) {
									int result;
									unsigned short cur_value, maximum;

									if ((result = ddcci_readctrl(mon, control->address, &cur_value, &maximum)) >= 0) {
										printf(
										    (result > 0)
										    ? _("\t\t  supported, value=%d, maximum=%d\n")
										    : _("\t\t  not supported, value=%d, maximum=%d\n"), cur_value, maximum);
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

			if (mon->fallback && !suppress_warning) {
				char *issue_url;

				report.fallback_profile = "yes";
				issue_url = build_issue_url(&report);

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
				           "Unsupported monitor detected.\n\n"
				           "Please update ddccontrol-db, or, if you are already using the latest\n"
				           "version, please open this pre-filled GitHub issue:\n"));
				printf("%s\n", issue_url ? issue_url : "https://github.com/ddccontrol/ddccontrol-db/issues/new?template=unsupported-monitor.yml");
				printf(_(
				           "Then attach the resulting report file of the following command:\n"));
				printf("\nLANG=C LC_ALL=C ddccontrol -p -c -d &> /tmp/ddccontrol-report.txt\n\n");
				printf(_("Thank you.\n"));
				printf("%s%s\n", _("=============================== WARNING ==============================="), isatty(1) ? "\x1B[0m" : "");

				free(issue_url);
			}
		}

		ddcci_close(mon);
		free(mon);
	}

	if (profilefile) {
		free(profilefile);
	}
	if (selected_need_free) {
		for (i = 0; i < selected_count; i++) {
			free(selected_fns[i]);
			free(selected_names[i]);
		}
	} else if (probe) {
		free(selected_monitor_name);
	}
	free(selected_fns);
	free(selected_names);

	ddcci_release();
	exit(0);
}
