#include "ddccontrol.h"

#include "internal.h"
#include "dbus_interface.h"

#include <stdio.h>

int perform_using_dbus(char *fn, int ctrl, int value) {
	unsigned int result_value;

	GError *error = NULL;
	DDCControl *proxy = ddccontrol_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			"ddccontrol.DDCControl",
			"/ddccontrol/DDCControl",
			NULL,
			&error
	);

	if( error != NULL ) {
		fprintf(stderr, _("D-Bus connection failed with error: %s.\n"), error->message);
		return -1;
	}

	if( value == -1 ) {
		gboolean result = ddccontrol_call_get_control_sync(proxy, fn, ctrl, &result_value, NULL, &error);
		// TODO: nicer output
		if(result == TRUE) {
			printf(_("Value: %d\n"), result_value);
		} else {
			printf(_("Read failed\n."));
		}
	} else {
		gboolean result = ddccontrol_call_set_control_sync(proxy, fn, ctrl, value, NULL, &error);
		if(result == FALSE) {
			printf(_("Write failed\n."));
		}
	}
	return 0;
}

