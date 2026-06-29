/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "../issueurl.h"
#include "../monitor_db.h"
#include "../monitor_db_internal.h"
#include "../urlencode.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DDCCONTROL_VERSION "3.2.0" /* x-release-please-version */

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
        .ddccontrol_version = TEST_DDCCONTROL_VERSION,
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
               "&ddccontrol_version=" TEST_DDCCONTROL_VERSION
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

static void test_monitor_db_parses_16bit_value(void) {
    char template[] = "/tmp/ddccontrol-test-XXXXXX";
    char *dir = mkdtemp(template);
    assert(dir != NULL);

    char monitor_dir[256];
    snprintf(monitor_dir, sizeof(monitor_dir), "%s/monitor", dir);
    assert(mkdir(monitor_dir, 0700) == 0);

    char options_path[256];
    snprintf(options_path, sizeof(options_path), "%s/options.xml", dir);
    FILE *options = fopen(options_path, "w");
    assert(options != NULL);
    fputs(
        "<options date='20260528' dbversion='3'>"
        "  <group name='Image'>"
        "    <subgroup name='Color'>"
        "      <control id='gamma' name='Gamma' type='list'>"
        "        <value id='wide' name='Wide gamut'/>"
        "      </control>"
        "    </subgroup>"
        "  </group>"
        "</options>",
        options);
    assert(fclose(options) == 0);

    char monitor_path[256];
    snprintf(monitor_path, sizeof(monitor_path), "%s/TST001.xml", monitor_dir);
    FILE *monitor_file = fopen(monitor_path, "w");
    assert(monitor_file != NULL);
    fputs(
        "<monitor name='Test Monitor' init='standard'>"
        "  <controls>"
        "    <control id='gamma' address='0x10'>"
        "      <value id='wide' value='40960'/>"
        "    </control>"
        "  </controls>"
        "</monitor>",
        monitor_file);
    assert(fclose(monitor_file) == 0);

    assert(ddcci_init_db(dir) == 1);
    struct caps caps = {0};
    assert(ddcci_parse_caps("(vcp(10))", &caps, 1) == 1);
    struct monitor_db *db = ddcci_create_db("TST001", &caps, 0);
    assert(db != NULL);
    assert(db->group_list != NULL);
    struct control_db *control = db->group_list->subgroup_list->control_list;
    assert(control != NULL);
    assert(control->value_list != NULL);
    assert(ddcci_value_db_value16(control->value_list) == 40960);
    assert(control->value_list->value == (40960 % 256));
    assert(control->value_list->next == NULL);

    ddcci_free_db(db);
    for (int i = 0; i < 256; i++) {
        if (caps.vcp[i]) {
            free(caps.vcp[i]->values);
            free(caps.vcp[i]);
        }
    }
    ddcci_release_db();

    assert(unlink(monitor_path) == 0);
    assert(unlink(options_path) == 0);
    assert(rmdir(monitor_dir) == 0);
    assert(rmdir(dir) == 0);
}

static void test_monitor_db_rejects_invalid_values(void) {
    char template[] = "/tmp/ddccontrol-test-invalid-XXXXXX";
    char *dir = mkdtemp(template);
    assert(dir != NULL);

    char monitor_dir[256];
    snprintf(monitor_dir, sizeof(monitor_dir), "%s/monitor", dir);
    assert(mkdir(monitor_dir, 0700) == 0);

    char options_path[256];
    snprintf(options_path, sizeof(options_path), "%s/options.xml", dir);
    FILE *options = fopen(options_path, "w");
    assert(options != NULL);
    fputs(
        "<options date='20260528' dbversion='3'>"
        "  <group name='Image'><subgroup name='Color'>"
        "    <control id='gamma' name='Gamma' type='list'>"
        "      <value id='wide' name='Wide gamut'/>"
        "    </control>"
        "  </subgroup></group>"
        "</options>",
        options);
    assert(fclose(options) == 0);

    const char *bad_values[] = {"65536", "-1"};
    for (size_t i = 0; i < sizeof(bad_values) / sizeof(bad_values[0]); i++) {
        char monitor_path[256];
        snprintf(monitor_path, sizeof(monitor_path), "%s/TST001.xml", monitor_dir);
        FILE *monitor_file = fopen(monitor_path, "w");
        assert(monitor_file != NULL);
        fprintf(
            monitor_file,
            "<monitor name='Test Monitor' init='standard'>"
            "  <controls><control id='gamma' address='0x10'>"
            "    <value id='wide' value='%s'/>"
            "  </control></controls>"
            "</monitor>",
            bad_values[i]);
        assert(fclose(monitor_file) == 0);

        assert(ddcci_init_db(dir) == 1);
        struct caps caps = {0};
        assert(ddcci_parse_caps("(vcp(10))", &caps, 1) == 1);
        assert(ddcci_create_db("TST001", &caps, 0) == NULL);
        for (int j = 0; j < 256; j++) {
            if (caps.vcp[j]) {
                free(caps.vcp[j]->values);
                free(caps.vcp[j]);
            }
        }
        ddcci_release_db();
        assert(unlink(monitor_path) == 0);
    }

    assert(unlink(options_path) == 0);
    assert(rmdir(monitor_dir) == 0);
    assert(rmdir(dir) == 0);
}

int main(void) {
    test_url_encode();
    test_build_issue_url_with_values();
    test_build_issue_url_with_null_fields();
    test_monitor_db_parses_16bit_value();
    test_monitor_db_rejects_invalid_values();
    return 0;
}
