#include "interface.h"
#include <stdio.h>
#include "ddcci.h"
#include "internal.h"

#define RETRYS 3 /* number of retrys */

static gchar * MONITORS[] = {
    "dev:/dev/i2c-3",
    "dev:/dev/i2c-7"
};

// TODO: duplicate in main.c
/* Find the delay we must respect after writing to an address in the database. */
static int find_write_delay(struct monitor* mon, char ctrl) {
	struct monitor_db* monitor = mon->db;
	struct group_db* group;
	struct subgroup_db* subgroup;
	struct control_db* control;

	if (monitor)
	{
		/* loop through groups */
		for (group = monitor->group_list; group != NULL; group = group->next)
		{
			/* loop through subgroups inside group */
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
			{
				/* loop through controls inside subgroup */
				for (control = subgroup->control_list; control != NULL; control = control->next)
				{
					/* check for control id */
					if (control->address == ctrl) 
					{
						return control->delay;					}
				}
			}
		}
	}
	return -1;
}

static gboolean handle_get_monitors(DDCControl *skeleton, GDBusMethodInvocation *invocation) {
    printf("DDCControl get monitors\n");
    ddccontrol_complete_get_monitors(skeleton, invocation, MONITORS);
    return TRUE;
}

static gboolean handle_get_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device, guint control) {

    int ret;
    unsigned short value, maximum;
    int retry, result;
    struct monitor mon;

    printf("DDCControl get %u for %s.\n", control, device);

    // TODO: keep monitors open for some time
    if ((ret = ddcci_open(&mon, device, 0)) < 0) {
        fprintf(stderr, 
                "\nDDC/CI at %s is unusable (%d).\n"
                "If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n",
                device,
                ret
        );
    } else {
	for (retry = RETRYS; retry; retry--) {
            result = ddcci_readctrl(&mon, control, &value, &maximum);
            if (result >= 0)
                break;
        }
    }

    ddccontrol_complete_get_control(skeleton, invocation, value);
    ddcci_close(&mon);
    return TRUE;
}

static gboolean handle_set_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device, guint control, guint value) {

    int ret;
    struct monitor mon;

    printf("DDCControl set %u on %s to %d.\n", control, device, value);

    // TODO: keep monitors open for some time
    if ((ret = ddcci_open(&mon, device, 0)) < 0) {
        fprintf(stderr,
                "\nDDC/CI at %s is unusable (%d).\n"
                "If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n",
                device,
                ret
        );
    } else {
        int delay = find_write_delay(&mon, control);
        if (delay >= 0) {
            fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d) (%dms delay)...\n"), control, value, value, delay);
        } else {
            fprintf(stdout, _("\nWriting 0x%02x, 0x%02x(%d)...\n"), control, value, value);
        }
        ddcci_writectrl(&mon, control, value, delay);
    }

    ddccontrol_complete_set_control(skeleton, invocation);
    ddccontrol_emit_control_changed(DDCCONTROL(skeleton), device, control, value);
    ddcci_close(&mon);
    return TRUE;
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    DDCControl *skeleton;

    skeleton = ddccontrol_skeleton_new();
    g_signal_connect(skeleton, "handle-get-monitors", G_CALLBACK(handle_get_monitors), NULL);
    g_signal_connect(skeleton, "handle-get-control",  G_CALLBACK(handle_get_control), NULL);
    g_signal_connect(skeleton, "handle-set-control",  G_CALLBACK(handle_set_control), NULL);

    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton), connection,
                                     "/ddccontrol/DDCControl", NULL);
}

void on_name_lost(GDBusConnection *connection,const gchar *name,gpointer user_data) {
    printf("Name lost\n");
}

int main(void) {
    GMainLoop *loop;

    if (!ddcci_init(NULL)) {
        printf(_("Unable to initialize ddcci library.\n"));
        exit(1);
    }

    loop = g_main_loop_new(NULL, FALSE);

    g_bus_own_name(G_BUS_TYPE_SYSTEM, "ddccontrol.DDCControl", G_BUS_NAME_OWNER_FLAGS_NONE,
                   NULL, on_name_acquired, on_name_lost,
                   NULL, NULL);

    g_main_loop_run(loop);

    ddcci_release();
    return 0;
}
