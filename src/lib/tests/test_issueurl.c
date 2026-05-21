#include "../issueurl.h"
#include "../monitor_db.h"
#include "../monitor_db_internal.h"
#include "../urlencode.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

static void free_value_list(struct value_db *value) {
    while (value != NULL) {
        struct value_db *next = value->next;
        xmlFree(value->id);
        xmlFree(value->name);
        free(value);
        value = next;
    }
}

static xmlNodePtr get_element(xmlNodePtr root, const char *name) {
    for (xmlNodePtr cur = root->children; cur != NULL; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, BAD_CAST name) == 0) {
            return cur;
        }
    }
    return NULL;
}

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

static void test_monitor_db_parses_16bit_value(void) {
    const char *xml =
        "<root>"
        "  <options_control><value id='gamma' name='Gamma mode'/></options_control>"
        "  <mon_control><value id='gamma' value='40960'/></mon_control>"
        "</root>";
    xmlDocPtr doc = xmlReadMemory(xml, (int)strlen(xml), "regression.xml", NULL, XML_PARSE_NONET);
    assert(doc != NULL);

    xmlNodePtr root = xmlDocGetRootElement(doc);
    assert(root != NULL);

    struct control_db control = {0};
    xmlNodePtr options = get_element(root, "options_control");
    xmlNodePtr mon = get_element(root, "mon_control");
    assert(options != NULL);
    assert(mon != NULL);

    int result = ddcci_get_value_list(options, mon, &control, 0, 0);
    assert(result == 0);
    assert(control.value_list != NULL);
    assert(control.value_list->value16 == 40960);
    assert(control.value_list->value == (40960 % 256));
    assert(control.value_list->next == NULL);

    free_value_list(control.value_list);
    xmlFreeDoc(doc);
}

static void test_monitor_db_rejects_invalid_values(void) {
    const char *xml_too_large =
        "<root>"
        "  <options_control><value id='gamma' name='Gamma mode'/></options_control>"
        "  <mon_control><value id='gamma' value='65536'/></mon_control>"
        "</root>";
    const char *xml_negative =
        "<root>"
        "  <options_control><value id='gamma' name='Gamma mode'/></options_control>"
        "  <mon_control><value id='gamma' value='-1'/></mon_control>"
        "</root>";
    const char *xml_missing_id =
        "<root>"
        "  <options_control><value id='gamma' name='Gamma mode'/></options_control>"
        "  <mon_control><value value='40960'/></mon_control>"
        "</root>";
    const char *inputs[] = {xml_too_large, xml_negative, xml_missing_id};

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        xmlDocPtr doc = xmlReadMemory(inputs[i], (int)strlen(inputs[i]), "regression.xml", NULL, XML_PARSE_NONET);
        assert(doc != NULL);

        xmlNodePtr root = xmlDocGetRootElement(doc);
        assert(root != NULL);

        struct control_db control = {0};
        xmlNodePtr options = get_element(root, "options_control");
        xmlNodePtr mon = get_element(root, "mon_control");
        assert(options != NULL);
        assert(mon != NULL);

        int result = ddcci_get_value_list(options, mon, &control, 0, 0);
        assert(result < 0);
        assert(control.value_list == NULL);

        xmlFreeDoc(doc);
    }
}

int main(void) {
    test_url_encode();
    test_build_issue_url_with_values();
    test_build_issue_url_with_null_fields();
    test_monitor_db_parses_16bit_value();
    test_monitor_db_rejects_invalid_values();
    return 0;
}
