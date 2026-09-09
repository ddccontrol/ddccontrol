/*
    C ABI declarations for ddccontrol's Rust components
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#ifndef DDCCONTROL_RUST_FFI_H
#define DDCCONTROL_RUST_FFI_H

#include "ddcci.h"
#include "conf.h"

#include <stddef.h>

struct ddccontrol_edid_result {
	char pnpid[8];
	unsigned char digital;
	unsigned char edid[DDCCI_EDID_BLOCK_LEN];
	int edid_len;
	struct edid_info info;
};

/* All output storage is owned by C. Rust writes result only on success. */
int ddccontrol_edid_parse(const unsigned char *buf, size_t len,
	struct ddccontrol_edid_result *result);

/* Rust allocates profile storage with C malloc; ddcci_free_profile releases it. */
struct profile *ddccontrol_profile_load(const char *filename);
int ddccontrol_profile_save(const struct profile *profile);

/* Cache paths and versions must be NUL-terminated. Both functions return 0
 * on success and -1 on failure, including a caught Rust panic.
 * Load writes output only on success (NULL for an empty cache); release the
 * malloc-allocated nodes and strings with ddcci_free_list.
 * Save borrows a valid acyclic list (NULL for empty) whose filename/name strings
 * are non-NULL, NUL-terminated UTF-8. Inputs remain caller-owned throughout.
 * Invalid list contents are rejected before opening the output file. */
int ddccontrol_monitorlist_load(const char *filename, const char *version,
	struct monitorlist **output);
int ddccontrol_monitorlist_save(const char *filename, const char *version,
	const struct monitorlist *list);

#endif /* DDCCONTROL_RUST_FFI_H */
