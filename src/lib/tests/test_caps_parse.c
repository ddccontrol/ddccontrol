#include "../ddcci.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void free_caps(struct caps *caps) {
    int i;
    for (i = 0; i < 256; i++) {
        if (caps->vcp[i]) {
            free(caps->vcp[i]->values);
            free(caps->vcp[i]);
        }
    }
}

static void test_uppercase_lcd_type(void) {
    struct caps caps;
    memset(&caps, 0, sizeof(caps));

    assert(ddcci_parse_caps("(prot(monitor)type(LCD)vcp(10))", &caps, 1) == 1);
    assert(caps.type == MONITOR_TYPE_LCD);

    free_caps(&caps);
}

static void test_uppercase_crt_type(void) {
    struct caps caps;
    memset(&caps, 0, sizeof(caps));

    assert(ddcci_parse_caps("(prot(monitor)type(CRT)vcp(10))", &caps, 1) == 1);
    assert(caps.type == MONITOR_TYPE_CRT);

    free_caps(&caps);
}

int main(void) {
    test_uppercase_lcd_type();
    test_uppercase_crt_type();
    return 0;
}
