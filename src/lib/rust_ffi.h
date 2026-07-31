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

#endif /* DDCCONTROL_RUST_FFI_H */
