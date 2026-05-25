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

int main(void) {
	test_valid_caps();
	test_rejects_overlong_value_token();
	test_rejects_unterminated_token();
	test_uppercase_lcd_type();
	test_uppercase_crt_type();
	return 0;
}
