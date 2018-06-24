/*
    ddc/ci command line tool
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

#include "interface.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ddcci.h"
#include "internal.h"

#define RETRYS 3 /* number of retrys */

// DDC Control D-Bus error declarations
#define DC_BUS_ERROR_OPEN_FAILED        "ddccontrol.DDCControl.Error.OpenFailed"
#define DC_BUS_ERROR_INVALID_DEVICE     "ddccontrol.DDCControl.Error.InvalidDevice"

static struct monitorlist *monlist = NULL;

static int devices_count = 0;
static char **devices = NULL;
static char *supported = NULL;
static char **names = NULL;
static char *digital = NULL;

static struct monitor *open_monitors = NULL;
static gboolean *monitor_open = NULL;
static int *monitor_ret = NULL;

static void rescan_monitors()
{
	int i, count;
	struct monitorlist *current;

	if (monlist != NULL) {
		free(devices);
		free(supported);
		free(names);
		free(digital);

		for (i = 0; i < devices_count; i++) {
			if (monitor_open[i] == TRUE)
				ddcci_close(&(open_monitors[i]));
		}
		free(open_monitors);
		free(monitor_open);
		free(monitor_ret);
		ddcci_free_list(monlist);
	}

	monlist = ddcci_probe();
	for (count = 0, current = monlist; current != NULL; current = current->next)
		count += 1;

	devices = malloc(sizeof(char *) * (count + 1));
	supported = malloc(sizeof(char) * (count));
	names = malloc(sizeof(char *) * (count + 1));
	digital = malloc(sizeof(char) * (count));
	open_monitors = malloc(sizeof(struct monitor) * (count));
	monitor_open = malloc(sizeof(gboolean) * (count));
	monitor_ret = malloc(sizeof(int) * (count));
	for (i = 0, current = monlist; current != NULL; current = current->next, i = i + 1) {
		devices[i] = current->filename;
		supported[i] = current->supported;
		monitor_open[i] = FALSE;
		names[i] = current->name;
		digital[i] = current->digital;
	}
	devices[i] = NULL;
	names[i] = NULL;
	devices_count = count;
}

// TODO: duplicate in main.c
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

static gboolean handle_get_monitors(DDCControl *skeleton, GDBusMethodInvocation *invocation)
{
	if (monlist == NULL)
		rescan_monitors();
	ddccontrol_complete_get_monitors(
	    skeleton,
	    invocation,
	    (const char **)devices,
	    g_variant_new_from_data(G_VARIANT_TYPE("a(y)"), supported, devices_count, TRUE, NULL, NULL),
	    (const char **)names,
	    g_variant_new_from_data(G_VARIANT_TYPE("a(y)"), digital, devices_count, TRUE, NULL, NULL)
	);
	return TRUE;
}

static gboolean handle_rescan_monitors(DDCControl *skeleton, GDBusMethodInvocation *invocation)
{
	rescan_monitors();
	ddccontrol_complete_rescan_monitors(
	    skeleton,
	    invocation,
	    (const char **)devices,
	    g_variant_new_from_data(G_VARIANT_TYPE("a(y)"), supported, devices_count, TRUE, NULL, NULL),
	    (const char **)names,
	    g_variant_new_from_data(G_VARIANT_TYPE("a(y)"), digital, devices_count, TRUE, NULL, NULL)
	);
	return TRUE;
}

static gboolean can_open_device(gchar *device)
{
	// THIS IS A SECURITY PRECAUTION
	int i;
	if (monlist != NULL && devices != NULL) {
		for (i = 0; i < devices_count; i++) {
			if (strcmp(devices[i], device) == 0)
				return TRUE;
		}
	}
	rescan_monitors();
	for (i = 0; i < devices_count; i++) {
		if (strcmp(devices[i], device) == 0)
			return TRUE;
	}
	return FALSE;
}

static int open_monitor(struct monitor **mon, const char *device)
{
	int i;
	for (i = 0; i < devices_count; i++) {
		if (strcmp(devices[i], device) == 0) {
			if (monitor_open[i] == FALSE) {
				monitor_ret[i] = ddcci_open(&(open_monitors[i]), device, 0);
				monitor_open[i] = TRUE;

				if (monitor_ret < 0) {
					fprintf(stderr,
					        "\nDDC/CI at %s is unusable (%d).\n"
					        "If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n",
					        device,
					        monitor_ret[i]
					       );
				}
			}
			*mon = &(open_monitors[i]);
			return monitor_ret[i];
		}
	}
	return -1;
}

static gboolean handle_open_monitor(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device)
{
	int ret;
	struct monitor *mon;

	if (can_open_device(device) == FALSE) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_INVALID_DEVICE,
		    "only detected devices are allowed"
		);
		return TRUE;
	}

	if ((ret = open_monitor(&mon, device)) < 0) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_OPEN_FAILED,
		    "Failed to open monitor"
		);
		return TRUE;
	}

	ddccontrol_complete_open_monitor(skeleton, invocation, mon->pnpid, mon->caps.raw_caps);
	return TRUE;
}

static gboolean handle_get_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                   gchar *device, guint control)
{

	int ret;
	unsigned short value, maximum;
	int retry, result;
	struct monitor *mon;

	printf("DDCControl get %u for %s.\n", control, device);

	if (can_open_device(device) == FALSE) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_INVALID_DEVICE,
		    "only detected devices are allowed"
		);
		return TRUE;
	}

	if ((ret = open_monitor(&mon, device)) < 0) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_OPEN_FAILED,
		    "Failed to open monitor"
		);
		return TRUE;
	}

	for (retry = RETRYS; retry; retry--) {
		result = ddcci_readctrl(mon, control, &value, &maximum);
		if (result >= 0)
			break;
	}


	ddccontrol_complete_get_control(skeleton, invocation, result, value, maximum);
	return TRUE;
}

static gboolean handle_set_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                   gchar *device, guint control, guint value)
{

	int ret;
	struct monitor *mon;

	printf("DDCControl set %u on %s to %d.\n", control, device, value);

	if (can_open_device(device) == FALSE) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_INVALID_DEVICE,
		    "only detected devices are allowed"
		);
		return TRUE;
	}

	if ((ret = open_monitor(&mon, device)) < 0) {
		g_dbus_method_invocation_return_dbus_error(
		    invocation,
		    DC_BUS_ERROR_OPEN_FAILED,
		    "Failed to open monitor"
		);
		return TRUE;
	}

	int delay = find_write_delay(mon, control);
	if (delay >= 0) {
		fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d) (%dms delay)...\n"), control, value, value, delay);
	} else {
		fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d)...\n"), control, value, value);
	}
	ddcci_writectrl(mon, control, value, delay);

	ddccontrol_complete_set_control(skeleton, invocation);
	ddccontrol_emit_control_changed(DDCCONTROL(skeleton), device, control, value);
	return TRUE;
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	DDCControl *skeleton;

	skeleton = ddccontrol_skeleton_new();
	g_signal_connect(skeleton, "handle-get-monitors", G_CALLBACK(handle_get_monitors), NULL);
	g_signal_connect(skeleton, "handle-rescan-monitors", G_CALLBACK(handle_rescan_monitors), NULL);
	g_signal_connect(skeleton, "handle-open-monitor", G_CALLBACK(handle_open_monitor), NULL);
	g_signal_connect(skeleton, "handle-get-control",  G_CALLBACK(handle_get_control), NULL);
	g_signal_connect(skeleton, "handle-set-control",  G_CALLBACK(handle_set_control), NULL);

	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton), connection,
	                                 "/ddccontrol/DDCControl", NULL);
}

void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	printf("Name lost\n");
}

static int is_i2c_loaded()
{
	FILE *pf;
	char result[128];

	pf = popen("lsmod | grep '^i2c_dev'", "r");
	fgets(result, 128, pf);

	if (pclose(pf) != 0)
		return FALSE;

	if (strncmp(result, "i2c_dev ", 8))
		return FALSE;

	return TRUE;
}

int load_i2c_dev()
{
	FILE *pf;

	fprintf(stderr, _("loading i2c-dev module: modprobe i2c-dev\n"));
	pf = popen("modprobe i2c-dev", "r");

	if (pclose(pf) != 0) {
		fprintf(stderr, _("Failed to load i2c-dev module, check output of `modprobe i2c-dev`!\n"));
		return FALSE;
	}

	return TRUE;
}

int check_or_load_i2c_dev()
{
	if (is_i2c_loaded() == FALSE) {
		fprintf(stderr, _("i2c-dev module isn't loaded\n"));
		return load_i2c_dev();
	}
	return TRUE;
}

int main(void)
{
	GMainLoop *loop;

	if (check_or_load_i2c_dev() == FALSE) {
		fprintf(stderr, _("Kernel module i2c_dev isn't available, functionality might be limited, or unavailable...\n"));
	}

	if (!ddcci_init(NULL)) {
		printf(_("Unable to initialize ddcci library.\n"));
		exit(1);
	}

	loop = g_main_loop_new(NULL, FALSE);

	g_bus_own_name(G_BUS_TYPE_SYSTEM, "ddccontrol.DDCControl", G_BUS_NAME_OWNER_FLAGS_NONE,
	               NULL, on_name_acquired, on_name_lost,
	               NULL, NULL);

	g_main_loop_run(loop);

	if (devices != NULL)
		ddcci_free_list(monlist);
	ddcci_release();
	return 0;
}
