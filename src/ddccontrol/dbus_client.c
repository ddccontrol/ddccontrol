#include "ddccontrol.h"

#include "internal.h"
#include "dbus_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dumpctrl(DDCControl *proxy, char *fn, struct monitor* mon, unsigned char ctrl, int force)
{
	unsigned short value, maximum;
	int result;
	GError *error = NULL;

	gboolean call_result = ddccontrol_call_get_control_sync(proxy, fn, ctrl, &result, &value, &maximum, NULL, &error);

	if(call_result == TRUE) {
		// TODO: mon parameter shouldn't be hard-coded NULL
		if ((result > 0) || force) {
			print_control_value(mon, ctrl, value, maximum, result);
		}
	} else {
		// TODO: better error output
		printf(_("Read failed: %s\n."), error->message);
	}
}

int perform_using_dbus(char *fn, int dump, int caps, int probe, int ctrl, int value) {
	int i;
	gboolean result;

	// TODO: custom datadir
	if (!ddcci_init_db(NULL)) {
		printf(_("Unable to initialize ddcci database.\n"));
		exit(1);
	}

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

	if (probe) {
		fn = NULL;

		char **devices = NULL, **names = NULL;
		char *supported = NULL, *digital = NULL;

		GVariant *v_supported, *v_digital;
		size_t supported_n, digital_n;

		result = ddccontrol_call_rescan_monitors_sync(proxy, &devices, &v_supported, &names, &v_digital, NULL, &error);

		printf(_("Detected monitors :\n"));
		if(result == FALSE) {
			// TODO: better error output
			fprintf(stderr, _("Probe failed: %s\n."), error->message);
			return -1;
		}

		supported = g_variant_get_fixed_array(v_supported, &supported_n, sizeof(char));
		digital = g_variant_get_fixed_array(v_digital, &digital_n, sizeof(char));


		for(i = 0; devices[i] != NULL && i < supported_n; i++) {
			printf(_(" - Device: %s\n"), devices[i]);
			printf(_("   DDC/CI supported: %s\n"), supported[i] ? _("Yes") : _("No"));
			printf(_("   Monitor Name: %s\n"), names[i]);
			printf(_("   Input type: %s\n"), digital[i] ? _("Digital") : _("Analog"));

			if ((!fn) && (supported[i]))
			{
				printf(_("   (Automatically selected)\n"));
				fn = malloc(strlen(devices[i])+1);
				strcpy(fn, devices[i]);
			}
		}

		if (fn == NULL) {
			fprintf(stderr, _(
				"No monitor supporting DDC/CI available.\n"
				"If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver).\n"
				));
			exit(0);
		}
	}

	struct monitor mon;
	char *pnpid = NULL;

	result = ddccontrol_call_open_monitor_sync(proxy, fn, &pnpid, &mon.caps.raw_caps, NULL, &error);
	if(result == FALSE) {
		// TODO: better error output
		fprintf(stderr, _("Open monitor failed: %s\n."), error->message);
		return -1;
	}

	strncpy(&mon.pnpid, pnpid, 7);
	mon.pnpid[8] = 0;

	ddcci_parse_caps(mon.caps.raw_caps, &mon.caps, 1);

	// TODO: duplicated from ddcci.c
	// DUPLICATED CODE START
	mon.db = ddcci_create_db(mon.pnpid, &mon.caps, 1);
	mon.fallback = 0; /* No fallback */

	if (!mon.db) {
		/* Fallback on manufacturer generic profile */
		char buffer[7];
		buffer[0] = 0;
		strncat(buffer, mon.pnpid, 3); /* copy manufacturer id */
		switch(mon.caps.type) {
		case lcd:
			strcat(buffer, "lcd");
			mon.db = ddcci_create_db(buffer, &mon.caps, 1);
			mon.fallback = 1;
			break;
		case crt:
			strcat(buffer, "crt");
			mon.db = ddcci_create_db(buffer, &mon.caps, 1);
			mon.fallback = 1;
			break;
		case unk:
			break;
		}

		if (!mon.db) {
			/* Fallback on VESA generic profile */
			mon.db = ddcci_create_db("VESA", &mon.caps, 1);
			mon.fallback = 2;
		}
	}
	// DUPLICATED CODE END


	if( ctrl >= 0 ) {
		if( value == -1 ) {
			fprintf(stdout, _("Reading 0x%02x...\n"), ctrl);
		} else {
			result = ddccontrol_call_set_control_sync(proxy, fn, ctrl, value, NULL, &error);
			if(result == FALSE) {
				printf(_("Write failed\n."));
			}
		}
		dumpctrl(proxy, fn, &mon, ctrl, 1);
	}

	if (dump) {
		fprintf(stdout, _("\nControls (valid/current/max) [Description - Value name]:\n"));

		for (i = 0; i < 256; i++) {
			dumpctrl(proxy, fn, &mon, i, 0 /* TODO: force flag */);
		}
	}
	return 0;
}

