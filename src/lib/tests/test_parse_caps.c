/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

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
	int initial_len;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(01)))", &caps, 1) == 1);
	assert(caps.vcp[0x10] != NULL);
	assert(caps.vcp[0x10]->values != NULL);
	initial_len = caps.vcp[0x10]->values_len;
	assert(initial_len > 0);

	assert(ddcci_parse_caps("(vcp(10(03)))", &caps, 0) == 1);
	assert(caps.vcp[0x10] != NULL);
	assert(caps.vcp[0x10]->values_len == initial_len);

	assert(ddcci_parse_caps("(vcp(10(01)))", &caps, 0) == 1);
	assert(caps.vcp[0x10] == NULL);

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
	assert(ddcci_parse_caps("(vcp(10(-1)))", &caps, 1) < 0);
	assert(ddcci_parse_caps("(vcp(10(10000)))", &caps, 1) < 0);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_rejects_out_of_range_vcp_identifier(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(-1))", &caps, 1) < 0);
	assert(caps.vcp[0x00] == NULL);

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

static void test_remove_nonexistent_control_with_value_list_is_safe(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10(01)))", &caps, 0) == 1);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_rejects_unbalanced_parentheses(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10)", &caps, 1) < 0);
	assert(ddcci_parse_caps(")(vcp(10))", &caps, 1) < 0);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_rejects_value_without_vcp_id(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp((01)))", &caps, 1) < 0);
	assert(caps.vcp[0x01] == NULL);
	assert(caps.vcp[0x10] == NULL);

	free_caps_entries(&caps);
}

static void test_boundary_control_and_value_ranges(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(00(0000) FF(FFFF)))", &caps, 1) == 2);
	assert(caps.vcp[0x00] != NULL);
	assert(caps.vcp[0x00]->values != NULL);
	assert(caps.vcp[0x00]->values_len == 1);
	assert(caps.vcp[0x00]->values[0] == 0x0000);
	assert(caps.vcp[0xFF] != NULL);
	assert(caps.vcp[0xFF]->values != NULL);
	assert(caps.vcp[0xFF]->values_len == 1);
	assert(caps.vcp[0xFF]->values[0] == 0xFFFF);

	free_caps_entries(&caps);
}

static void test_repeated_same_control_in_single_string(void) {
	struct caps caps;
	memset(&caps, 0, sizeof(caps));

	assert(ddcci_parse_caps("(vcp(10 10 10))", &caps, 1) == 3);
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
	test_rejects_out_of_range_vcp_identifier();
	test_nested_sections_still_allow_top_level_vcp();
	test_unknown_type_does_not_override_existing_type();
	test_repeated_add_remove_sequence_on_same_control();
	test_remove_nonexistent_control_with_value_list_is_safe();
	test_rejects_unbalanced_parentheses();
	test_rejects_value_without_vcp_id();
	test_boundary_control_and_value_ranges();
	test_repeated_same_control_in_single_string();
	return 0;
}
