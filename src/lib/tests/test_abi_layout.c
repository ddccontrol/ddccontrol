/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "../ddcci.h"
#include "../monitor_db_internal.h"
#include "../rust_ffi.h"

#include <stddef.h>

#define ABI_ASSERT_NAME2(prefix, line) prefix##line
#define ABI_ASSERT_NAME(prefix, line) ABI_ASSERT_NAME2(prefix, line)
#define ABI_ASSERT(cond) typedef char ABI_ASSERT_NAME(ddccontrol_abi_assert_, __LINE__)[(cond) ? 1 : -1]

ABI_ASSERT(sizeof(enum monitor_type) == sizeof(int));
ABI_ASSERT(sizeof(enum control_type) == sizeof(int));
ABI_ASSERT(sizeof(enum refresh_type) == sizeof(int));
ABI_ASSERT(sizeof(enum init_type) == sizeof(int));

ABI_ASSERT(offsetof(struct vcp_entry, values_len) == 0);
ABI_ASSERT(offsetof(struct caps, vcp) == 0);
ABI_ASSERT(offsetof(struct value_db, id) == 0);
ABI_ASSERT(offsetof(struct monitor_db, name) == 0);
ABI_ASSERT(offsetof(struct value_db_private, public_value) == 0);
ABI_ASSERT(sizeof(unsigned int) == 4);
ABI_ASSERT(offsetof(struct edid_info, serial_number) == 0);
ABI_ASSERT(offsetof(struct edid_info, manufacture_week) == 4);
ABI_ASSERT(offsetof(struct edid_info, manufacture_year) == 8);
ABI_ASSERT(offsetof(struct edid_info, version) == 12);
ABI_ASSERT(offsetof(struct edid_info, revision) == 16);
ABI_ASSERT(offsetof(struct edid_info, max_width_cm) == 20);
ABI_ASSERT(offsetof(struct edid_info, max_height_cm) == 24);
ABI_ASSERT(offsetof(struct edid_info, monitor_name) == 28);
ABI_ASSERT(offsetof(struct edid_info, serial_ascii) == 42);
ABI_ASSERT(offsetof(struct ddccontrol_edid_result, pnpid) == 0);
ABI_ASSERT(offsetof(struct ddccontrol_edid_result, digital) == 8);
ABI_ASSERT(offsetof(struct ddccontrol_edid_result, edid) == 9);
ABI_ASSERT(offsetof(struct ddccontrol_edid_result, edid_len) == 140);
ABI_ASSERT(offsetof(struct ddccontrol_edid_result, info) == 144);

int main(void)
{
	return 0;
}
