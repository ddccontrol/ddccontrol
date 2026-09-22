/*
    Internal C ABI for the pure Rust DDC/CI protocol functions
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#ifndef DDCCI_PROTOCOL_H
#define DDCCI_PROTOCOL_H

#include <stddef.h>

enum {
	DDCCI_MAX_PAYLOAD_LEN = 127,
	DDCCI_MAX_FRAME_LEN = 130,
	DDCCI_PROTOCOL_ERROR = -1,
	DDCCI_PROTOCOL_ADDRESS = -2,
	DDCCI_PROTOCOL_LENGTH = -3,
	DDCCI_PROTOCOL_CHECKSUM = -4
};

/* All buffers belong to C; no allocation or ownership crosses this ABI.
 * Input/output storage must be valid for the specified lengths and must not
 * overlap. Outputs are left untouched on error. Addresses are seven-bit I2C
 * addresses. Negative results are errors, nonnegative results are lengths
 * except for parse_vcp (documented below). */
int ddccontrol_ddcci_frame_length(size_t payload_len);
int ddccontrol_ddcci_build_frame(unsigned int address,
	const unsigned char *payload, size_t payload_len,
	unsigned char *frame, size_t frame_capacity);

/* Only min(frame_len, DDCCI_MAX_FRAME_LEN) input bytes are inspected.
 * payload may be NULL with zero capacity. missing_length_flag is optional;
 * on success it receives 1 for the tolerated missing length flag, else 0. */
int ddccontrol_ddcci_parse_frame(unsigned int address,
	const unsigned char *frame, size_t frame_len,
	unsigned char *payload, size_t payload_capacity, int *missing_length_flag);

/* Returns 1 for supported, 0 for unsupported, -1 for malformed replies.
 * value and maximum are optional and are written even for unsupported VCPs. */
int ddccontrol_ddcci_parse_vcp(const unsigned char *payload, size_t payload_len,
	unsigned char control, unsigned short *value, unsigned short *maximum);

/* Returns fragment length (zero marks completion) or -1 for malformed replies.
 * On success the fragment starts at payload + 3; embedded NULs are preserved. */
int ddccontrol_ddcci_parse_caps_reply(const unsigned char *payload,
	size_t payload_len, unsigned int offset);

#endif /* DDCCI_PROTOCOL_H */
