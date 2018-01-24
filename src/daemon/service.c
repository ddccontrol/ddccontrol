#include "interface.h"
#include <stdio.h>

static gchar * MONITORS[] = {
    "dev:/dev/i2c-3",
    "dev:/dev/i2c-7"
};

static gboolean handle_get_monitors(DDCControl *skeleton, GDBusMethodInvocation *invocation) {
    printf("DDCControl get monitors\n");
    ddccontrol_complete_get_monitors(skeleton, invocation, MONITORS);
    return TRUE;
}

static gboolean handle_get_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device, guint control) {

    printf("DDCControl get %u for %s.\n", control, device);
    ddccontrol_complete_get_control(skeleton, invocation, 47);
    return TRUE;
}

static gboolean handle_set_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device, guint control, guint value) {

    printf("DDCControl set %u on %s to %d.\n", control, device, value);
    ddccontrol_complete_set_control(skeleton, invocation);
    ddccontrol_emit_control_changed(DDCCONTROL(skeleton), device, control, value);
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

void main(void) {
    GMainLoop *loop;

    loop = g_main_loop_new(NULL, FALSE);

    g_bus_own_name(G_BUS_TYPE_SESSION, "ddccontrol.DDCControl", G_BUS_NAME_OWNER_FLAGS_NONE,
                   NULL, on_name_acquired, NULL,
                   NULL, NULL);

    g_main_loop_run(loop);
}
