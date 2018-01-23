
#include "interface.h"
#include <stdio.h>

static GMainLoop *loop;

void control_changed_callback(DDCControl *proxy, gchar *device, guint control, guint value) {
    printf("Control %d changed for %s to %d\n", control, device, value);
    g_main_loop_quit(loop);
}

int main(void) {
    loop = g_main_loop_new(NULL, FALSE);

    GError *error = NULL;
    DDCControl *proxy = ddccontrol_proxy_new_for_bus_sync(
            G_BUS_TYPE_SESSION,
            G_DBUS_PROXY_FLAGS_NONE,
            "ddccontrol.DDCControl",
            "/ddccontrol/DDCControl",
            NULL,
            &error
    );
    g_signal_connect(proxy, "control-changed", G_CALLBACK(control_changed_callback), NULL);

    // set control
    printf("Setting control\n");
    gboolean result = ddccontrol_call_set_control_sync(proxy, "dev:/dev/i2c-3", 0x12, 50, NULL, &error);
    printf("Done setting control\n");

    g_main_loop_run(loop);
    printf("exiting program.\n");
    g_object_unref(proxy);

    return 0;
}
