/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "interface.h"
#include "ddcci.h"

#include <string.h>

#define TEST_DEVICE "dev:/dev/test-ddccontrol"

static GVariant *captured_reply;
static const gchar *captured_error;
static char *device_caps;
static int device_result;
static int device_open_calls;

/* Model the result of hardware access, including a successful legacy-presence
 * fallback after failed CAPS retrieval (success with raw_caps == NULL). */
static int test_ddcci_open(struct monitor *mon, const char *device, int probing)
{
	g_assert_cmpstr(device, ==, TEST_DEVICE);
	g_assert_cmpint(probing, ==, 0);
	device_open_calls++;
	memset(mon, 0, sizeof(*mon));
	strcpy(mon->pnpid, "DEL0000");
	mon->caps.raw_caps = device_caps;
	return device_result;
}

/* Capture the real generated serializer's output without a running bus. */
static void capture_return_value(GDBusMethodInvocation *invocation, GVariant *value)
{
	g_assert(invocation == NULL);
	g_assert(captured_reply == NULL);
	captured_reply = g_variant_ref_sink(value);
}

static void capture_return_error(GDBusMethodInvocation *invocation,
                                 const gchar *name, const gchar *message)
{
	g_assert(invocation == NULL);
	g_assert(message != NULL);
	g_assert(captured_error == NULL);
	captured_error = name;
}

#define g_dbus_method_invocation_return_value capture_return_value
#include "interface.c"
#undef g_dbus_method_invocation_return_value

#define main daemon_program_main
#define ddcci_open test_ddcci_open
#define g_dbus_method_invocation_return_dbus_error capture_return_error
#include "../service.c"
#undef g_dbus_method_invocation_return_dbus_error
#undef ddcci_open
#undef main

static void prepare_monitor(char *caps, int result)
{
	static char *test_devices[] = {TEST_DEVICE, NULL};
	static struct monitor test_monitor;
	static gboolean test_open;
	static int test_result;

	g_assert(captured_reply == NULL);
	captured_error = NULL;
	device_caps = caps;
	device_result = result;
	device_open_calls = 0;
	memset(&test_monitor, 0, sizeof(test_monitor));
	test_open = FALSE;
	test_result = 0;
	devices_count = 1;
	devices = test_devices;
	open_monitors = &test_monitor;
	monitor_open = &test_open;
	monitor_ret = &test_result;
}

static void assert_reply(const char *expected_caps)
{
	const gchar *pnpid, *caps;
	g_assert(captured_error == NULL);
	g_assert(captured_reply != NULL);
	g_assert(g_variant_is_of_type(captured_reply, G_VARIANT_TYPE("(ss)")));
	g_variant_get(captured_reply, "(&s&s)", &pnpid, &caps);
	g_assert_cmpstr(pnpid, ==, "DEL0000");
	g_assert_cmpstr(caps, ==, expected_caps);
	g_variant_unref(captured_reply);
	captured_reply = NULL;
}

static void test_open_after_caps_failure(void)
{
	prepare_monitor(NULL, 0);
	g_assert(handle_open_monitor(NULL, NULL, TEST_DEVICE));
	g_assert_cmpint(device_open_calls, ==, 1);
	g_assert(open_monitors[0].caps.raw_caps == NULL);
	assert_reply("");

	/* A subsequent request reuses the cached monitor and still returns valid
	 * D-Bus string arguments without altering the monitor's ownership. */
	g_assert(handle_open_monitor(NULL, NULL, TEST_DEVICE));
	g_assert_cmpint(device_open_calls, ==, 1);
	g_assert(open_monitors[0].caps.raw_caps == NULL);
	assert_reply("");
}

static void test_open_preserves_caps(void)
{
	char caps[] = "(prot(monitor)type(lcd)vcp(10 12))";
	prepare_monitor(caps, 0);
	g_assert(handle_open_monitor(NULL, NULL, TEST_DEVICE));
	g_assert(open_monitors[0].caps.raw_caps == caps);
	assert_reply(caps);
}

static void test_open_with_empty_caps(void)
{
	char caps[] = "";
	prepare_monitor(caps, 0);
	g_assert(handle_open_monitor(NULL, NULL, TEST_DEVICE));
	assert_reply("");
}

static void test_open_failure_returns_error(void)
{
	prepare_monitor(NULL, -1);
	g_assert(handle_open_monitor(NULL, NULL, TEST_DEVICE));
	g_assert_cmpint(device_open_calls, ==, 1);
	g_assert(captured_reply == NULL);
	g_assert_cmpstr(captured_error, ==, DC_BUS_ERROR_OPEN_FAILED);
}

int main(int argc, char **argv)
{
	/* g_test_init makes GLib criticals fatal, catching invalid string arguments. */
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/daemon/open-monitor/caps-failure", test_open_after_caps_failure);
	g_test_add_func("/daemon/open-monitor/valid-caps", test_open_preserves_caps);
	g_test_add_func("/daemon/open-monitor/empty-caps", test_open_with_empty_caps);
	g_test_add_func("/daemon/open-monitor/open-failure", test_open_failure_returns_error);
	return g_test_run();
}
