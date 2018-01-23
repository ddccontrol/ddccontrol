#include "interface.h"
#include <stdio.h>

static gboolean handle_set_control(DDCControl *skeleton, GDBusMethodInvocation *invocation,
                                    gchar *device, guint control, guint value) {

    printf("DDCControl set %d on %s to %d.\n", control, device, value);
    ddccontrol_complete_set_control(skeleton, invocation);
    ddccontrol_emit_control_changed(DDCCONTROL(skeleton), device, control, value);
    return TRUE;
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    DDCControl *skeleton;

    skeleton = ddccontrol_skeleton_new();
    g_signal_connect(skeleton, "handle-set-control", G_CALLBACK(handle_set_control), NULL);

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
