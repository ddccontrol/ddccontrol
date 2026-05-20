#include "../issueurl.h"
#include "../monitor_db.h"
#include "../urlencode.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void assert_encoded(const char *input, const char *expected) {
    char *encoded = url_encode(input);
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);
}

static void test_url_encode(void) {
    assert(url_encode(NULL) == NULL);

    assert_encoded("", "");
    assert_encoded("abcXYZ012-_.~", "abcXYZ012-_.~");
    assert_encoded("hello world!", "hello%20world%21");
    assert_encoded("foo/bar?x=1&y=2", "foo%2Fbar%3Fx%3D1%26y%3D2");
    assert_encoded("100%", "100%25");
}

static void test_build_issue_url_with_values(void) {
    struct issue_report report = {
        .monitor_name = "Dell U2720Q",
        .pnp_id = "DEL1234",
        .device = "dev:/dev/i2c-4",
        .ddccontrol_version = "1.0.3",
        .fallback_profile = "VESA Monitor",
    };
    char *url = build_issue_url(&report);
    assert(url != NULL);

    assert(strcmp(
               url,
               "https://github.com/ddccontrol/ddccontrol-db/issues/new"
               "?template=unsupported-monitor.yml"
               "&monitor_name=Dell%20U2720Q"
               "&pnp_id=DEL1234"
               "&device=dev%3A%2Fdev%2Fi2c-4"
               "&ddccontrol_version=1.0.3"
               "&fallback_profile=VESA%20Monitor") == 0);
    free(url);
}

static void test_build_issue_url_with_null_fields(void) {
    struct issue_report report = {
        .monitor_name = NULL,
        .pnp_id = NULL,
        .device = NULL,
        .ddccontrol_version = NULL,
        .fallback_profile = NULL,
    };
    char *url = build_issue_url(&report);
    assert(url != NULL);

    assert(strcmp(
               url,
               "https://github.com/ddccontrol/ddccontrol-db/issues/new"
               "?template=unsupported-monitor.yml"
               "&monitor_name="
               "&pnp_id="
               "&device="
               "&ddccontrol_version="
               "&fallback_profile=") == 0);
    free(url);
}

static void test_monitor_value_width(void) {
    struct value_db value = {0};
    value.value = 40960;
    assert(value.value == 40960);
}

int main(void) {
    test_url_encode();
    test_build_issue_url_with_values();
    test_build_issue_url_with_null_fields();
    test_monitor_value_width();
    return 0;
}
