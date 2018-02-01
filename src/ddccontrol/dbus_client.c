#include "ddccontrol.h"

#include "internal.h"
#include "dbus_interface.h"

#include <stdio.h>

int perform_using_dbus(char *fn, int ctrl, int value) {
	unsigned int result_value, result_maximum;

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
		fprintf(stdout, _("Reading 0x%02x...\n"), ctrl);
		gboolean result = ddccontrol_call_get_control_sync(proxy, fn, ctrl, &result_value, &result_maximum, NULL, &error);
		if(result == TRUE) {
			// TODO: mon parameter shouldn't be hard-coded NULL
			print_control_value(NULL, ctrl, result_value, result_maximum, 1);
		} else {
			// TODO: better error output
			printf(_("Read failed: %s\n."), error->message);
			return -1;
		}
	} else {
		gboolean result = ddccontrol_call_set_control_sync(proxy, fn, ctrl, value, NULL, &error);
		if(result == FALSE) {
			printf(_("Write failed\n."));
		}
	}
	return 0;
}

