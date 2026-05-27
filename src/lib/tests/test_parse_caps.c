#include "../ddcci.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void free_caps_entries(struct caps *caps) {
	int i;
	for (i = 0; i < 256; i++) {
		if (caps->vcp[i]) {
			free(caps->vcp[i]->values);
			free(caps->vcp[i]);
			caps->vcp[i] = NULL;
		}
	}
}

static void test_valid_caps(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));
	assert(ddcci_parse_caps("(vcp(10 12))", &caps, 1) == 2);
	free_caps_entries(&caps);
}

static void test_rejects_overlong_value_token(void) {
	struct caps caps;
	char long_caps[256];

	memset(&caps, 0, sizeof(caps));
	memset(long_caps, 'A', sizeof(long_caps));
	memcpy(long_caps, "(vcp(10(", 8);
	memset(long_caps + 8, '1', 240);
	memcpy(long_caps + 248, ")))", 4);

	assert(ddcci_parse_caps(long_caps, &caps, 1) < 0);
	free_caps_entries(&caps);
}

static void test_rejects_unterminated_token(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(12", &caps, 1) < 0);
	free_caps_entries(&caps);
}

static void test_uppercase_lcd_type(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(prot(monitor)type(LCD))", &caps, 1) >= 0);
	assert(caps.type == lcd);

	free_caps_entries(&caps);
}

static void test_uppercase_crt_type(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(prot(monitor)type(CRT))", &caps, 1) >= 0);
	assert(caps.type == crt);

	free_caps_entries(&caps);
}

static void test_add_values_then_remove_one_value(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(01 02)))", &caps, 1) == 1);
	assert(caps.vcp[0x10] != NULL);
	assert(caps.vcp[0x10]->values != NULL);

	assert(ddcci_parse_caps("(vcp(10(01)))", &caps, 0) == 1);
	assert(caps.vcp[0x10] != NULL);

	free_caps_entries(&caps);
}

static void test_remove_whole_control_without_value_list(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10 12))", &caps, 1) == 2);
	assert(caps.vcp[0x10] != NULL);
	assert(caps.vcp[0x12] != NULL);

	assert(ddcci_parse_caps("(vcp(10))", &caps, 0) == 1);
	assert(caps.vcp[0x10] == NULL);
	assert(caps.vcp[0x12] != NULL);

	free_caps_entries(&caps);
}

static void test_rejects_invalid_vcp_identifier(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(GG))", &caps, 1) < 0);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_remove_nonexistent_control_is_safe(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10))", &caps, 0) == 1);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_mixed_controls_and_value_lists(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(01 02) 12 14(0A)))", &caps, 1) == 3);
	assert(caps.vcp[0x10] != NULL);
	assert(caps.vcp[0x10]->values != NULL);
	assert(caps.vcp[0x12] != NULL);
	assert(caps.vcp[0x14] != NULL);
	assert(caps.vcp[0x14]->values != NULL);

	free_caps_entries(&caps);
}

static void test_rejects_invalid_hex_value_in_value_list(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(0G)))", &caps, 1) < 0);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_nested_sections_still_allow_top_level_vcp(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(prot(monitor)foo(bar(baz))vcp(10))", &caps, 1) == 1);
	assert(caps.vcp[0x10] != NULL);

	free_caps_entries(&caps);
}

static void test_unknown_type_does_not_override_existing_type(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));
	caps.type = lcd;

	assert(ddcci_parse_caps("(type(OLED))", &caps, 1) >= 0);
	assert(caps.type == lcd);

	free_caps_entries(&caps);
}

static void test_repeated_add_remove_sequence_on_same_control(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(01 02)))", &caps, 1) == 1);
	assert(caps.vcp[0x10] != NULL);

	assert(ddcci_parse_caps("(vcp(10(01)))", &caps, 0) == 1);
	assert(caps.vcp[0x10] != NULL);

	assert(ddcci_parse_caps("(vcp(10))", &caps, 0) == 1);
	assert(caps.vcp[0x10] == NULL);

	assert(ddcci_parse_caps("(vcp(10))", &caps, 1) == 1);
	assert(caps.vcp[0x10] != NULL);

	free_caps_entries(&caps);
}

int main(void) {
	test_valid_caps();
	test_rejects_overlong_value_token();
	test_rejects_unterminated_token();
	test_uppercase_lcd_type();
	test_uppercase_crt_type();
	test_add_values_then_remove_one_value();
	test_remove_whole_control_without_value_list();
	test_rejects_invalid_vcp_identifier();
	test_remove_nonexistent_control_is_safe();
	test_mixed_controls_and_value_lists();
	test_rejects_invalid_hex_value_in_value_list();
	test_nested_sections_still_allow_top_level_vcp();
	test_unknown_type_does_not_override_existing_type();
	test_repeated_add_remove_sequence_on_same_control();
	return 0;
}
