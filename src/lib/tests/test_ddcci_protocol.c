/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "config.h"
#include "../ddcci_protocol.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static const unsigned char vcp_frame[] = {
	0x6e, 0x88, 0x02, 0, 0x10, 0, 0x12, 0x34, 0xab, 0xcd, 0xe4
};
static const unsigned char read_request[] = {0x51, 0x82, 0x01, 0x10, 0xac};
static const unsigned char write_request[] = {0x51, 0x84, 0x03, 0x10, 0x12, 0x34, 0x8e};

static void assert_filled(const unsigned char *buf, size_t len, unsigned char value)
{
	size_t i;
	for (i = 0; i < len; i++)
		assert(buf[i] == value);
}

static void test_build_frame(void)
{
	unsigned char payload[128], frame[DDCCI_MAX_FRAME_LEN + 1];
	size_t len;
	memset(payload, 0x23, sizeof(payload));
	for (len = 0; len <= DDCCI_MAX_PAYLOAD_LEN; len++) {
		memset(frame, 0xa5, sizeof(frame));
		assert(ddccontrol_ddcci_frame_length(len) == (int)len + 3);
		assert(ddccontrol_ddcci_build_frame(0x37, payload, len, frame, len + 3) == (int)len + 3);
		assert(frame[1] == (0x80 | len));
		assert(memcmp(frame + 2, payload, len) == 0);
		assert(frame[len + 3] == 0xa5);
	}
	assert(ddccontrol_ddcci_build_frame(0x37, read_request + 2, 2, frame, sizeof(frame)) == 5);
	assert(memcmp(frame, read_request, sizeof(read_request)) == 0);
	assert(ddccontrol_ddcci_build_frame(0x37, write_request + 2, 4, frame, sizeof(frame)) == 7);
	assert(memcmp(frame, write_request, sizeof(write_request)) == 0);
	memset(frame, 0xa5, sizeof(frame));
	assert(ddccontrol_ddcci_build_frame(0x37, payload, 128, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(0x37, payload, (size_t)-1, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(0x37, payload, 127, frame, 129) < 0);
	assert(ddccontrol_ddcci_build_frame(0x80, payload, 1, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(0x100, payload, 1, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(UINT_MAX, payload, 1, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(0x37, NULL, 1, frame, sizeof(frame)) < 0);
	assert(ddccontrol_ddcci_build_frame(0x37, payload, 1, NULL, sizeof(frame)) < 0);
	assert_filled(frame, sizeof(frame), 0xa5);
	assert(ddccontrol_ddcci_build_frame(0x37, NULL, 0, frame, sizeof(frame)) == 3);
	assert(frame[0] == 0x51 && frame[1] == 0x80 && frame[2] == 0xbf);
	assert(ddccontrol_ddcci_frame_length(128) < 0);
	assert(ddccontrol_ddcci_frame_length((size_t)-1) < 0);
}

static void test_parse_frame(void)
{
	unsigned char frame[DDCCI_MAX_FRAME_LEN], payload[DDCCI_MAX_PAYLOAD_LEN];
	int missing = 7;
	size_t len;
	memset(payload, 0xa5, sizeof(payload));
	for (len = 0; len < sizeof(vcp_frame); len++) {
		assert(ddccontrol_ddcci_parse_frame(0x37, vcp_frame, len, payload, sizeof(payload), &missing) == DDCCI_PROTOCOL_LENGTH);
		assert(missing == 7);
		assert_filled(payload, sizeof(payload), 0xa5);
	}
	assert(ddccontrol_ddcci_parse_frame(0x37, vcp_frame, sizeof(vcp_frame), payload, 7, &missing) == DDCCI_PROTOCOL_LENGTH);
	assert(ddccontrol_ddcci_parse_frame(0x38, vcp_frame, sizeof(vcp_frame), payload, sizeof(payload), &missing) == DDCCI_PROTOCOL_ADDRESS);
	assert(ddccontrol_ddcci_parse_frame(UINT_MAX, vcp_frame, sizeof(vcp_frame), payload, sizeof(payload), &missing) < 0);
	assert(ddccontrol_ddcci_parse_frame(0x37, NULL, 0, payload, sizeof(payload), &missing) < 0);
	assert(ddccontrol_ddcci_parse_frame(0x37, vcp_frame, sizeof(vcp_frame), NULL, 8, &missing) < 0);
	memset(frame, 0xff, sizeof(frame));
	memcpy(frame, vcp_frame, sizeof(vcp_frame));
	frame[10] ^= 1;
	assert(ddccontrol_ddcci_parse_frame(0x37, frame, sizeof(frame), payload, sizeof(payload), &missing) == DDCCI_PROTOCOL_CHECKSUM);
	assert(missing == 7);
	assert_filled(payload, sizeof(payload), 0xa5);
	frame[10] ^= 1;
	assert(ddccontrol_ddcci_parse_frame(0x37, frame, sizeof(frame), payload, sizeof(payload), &missing) == 8);
	assert(memcmp(payload, vcp_frame + 2, 8) == 0 && missing == 0);
	assert_filled(payload + 8, sizeof(payload) - 8, 0xa5);
	frame[1] ^= 0x80;
	frame[10] ^= 0x80;
	assert(ddccontrol_ddcci_parse_frame(0x37, frame, sizeof(frame), payload, 8, &missing) == 8);
	assert(missing == 1);
	frame[0] = 0x6e;
	frame[1] = 0x80;
	frame[2] = 0xbe;
	assert(ddccontrol_ddcci_parse_frame(0x37, frame, 3, NULL, 0, NULL) == 0);
}

static void test_reply_parsers(void)
{
	unsigned char payload[8];
	unsigned short value = 7, maximum = 9;
	static const unsigned char caps[] = {0xe3, 0x12, 0x34, 'A', 0, ')', 0xff};
	memcpy(payload, vcp_frame + 2, sizeof(payload));
	assert(ddccontrol_ddcci_parse_vcp(payload, 7, 0x10, &value, &maximum) == -1);
	assert(ddccontrol_ddcci_parse_vcp(payload, (size_t)-1, 0x10, &value, &maximum) == -1);
	assert(ddccontrol_ddcci_parse_vcp(NULL, 8, 0x10, &value, &maximum) == -1);
	assert(ddccontrol_ddcci_parse_vcp(payload, 8, 0x12, &value, &maximum) == -1);
	assert(value == 7 && maximum == 9);
	assert(ddccontrol_ddcci_parse_vcp(payload, 8, 0x10, &value, &maximum) == 1);
	assert(value == 0xabcd && maximum == 0x1234);
	payload[1] = 0xff;
	value = maximum = 0;
	assert(ddccontrol_ddcci_parse_vcp(payload, 8, 0x10, &value, &maximum) == 0);
	assert(value == 0xabcd && maximum == 0x1234);
	assert(ddccontrol_ddcci_parse_vcp(payload, 8, 0x10, NULL, NULL) == 0);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, sizeof(caps), 0x1234) == 4);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, 3, 0x1234) == 0);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, 3, 0x3412) == -1);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, 3, 0x11234) == -1);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, 2, 0) == -1);
	assert(ddccontrol_ddcci_parse_caps_reply(caps, (size_t)-1, 0) == -1);
	assert(ddccontrol_ddcci_parse_caps_reply(NULL, 3, 0) == -1);
}

#ifdef HAVE_I2C_DEV

#include <stdarg.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../ddcci.h"
#include "../i2c-dev.h"

static unsigned char device_reply[DDCCI_MAX_FRAME_LEN], device_request[DDCCI_MAX_FRAME_LEN];
static size_t device_request_len;
static int reads, writes;

static int protocol_test_ioctl(int fd, unsigned long request, ...)
{
	va_list args;
	struct i2c_rdwr_ioctl_data *transfer;
	struct i2c_msg *message;
	assert(fd == 123 && request == I2C_RDWR);
	va_start(args, request);
	transfer = va_arg(args, struct i2c_rdwr_ioctl_data *);
	va_end(args);
	assert(transfer->nmsgs == 1);
	message = transfer->msgs;
	assert(message->len <= DDCCI_MAX_FRAME_LEN);
	if (message->flags & I2C_M_RD) {
		memcpy(message->buf, device_reply, message->len);
		reads++;
	} else {
		memcpy(device_request, message->buf, message->len);
		device_request_len = message->len;
		writes++;
	}
#ifdef __FreeBSD__
	return 0;
#else
	return 1;
#endif
}

static int protocol_test_usleep(useconds_t usec)
{
	(void)usec;
	return 0;
}

/* Exercise the production C callers and Rust ABI without a physical monitor. */
#define ioctl protocol_test_ioctl
#define usleep protocol_test_usleep
#include "../ddcci.c"
#undef ioctl
#undef usleep

static void test_transport_callers(void)
{
	struct monitor mon;
	unsigned char payload[DDCCI_MAX_PAYLOAD_LEN];
	unsigned short value = 0, maximum = 0;
	size_t i;
	memset(&mon, 0, sizeof(mon));
	mon.type = dev;
	mon.addr = 0x37;
	mon.fd = 123;
	mon.probing = 1;
	memcpy(device_reply, vcp_frame, sizeof(vcp_frame));
	assert(ddcci_readctrl(&mon, 0x10, &value, &maximum) == 1);
	assert(value == 0xabcd && maximum == 0x1234);
	assert(reads == 1 && writes == 1);
	assert(device_request_len == sizeof(read_request));
	assert(memcmp(device_request, read_request, sizeof(read_request)) == 0);
	assert(ddcci_writectrl(&mon, 0x10, 0x1234, 0) >= 0);
	assert(device_request_len == sizeof(write_request));
	assert(memcmp(device_request, write_request, sizeof(write_request)) == 0);
	assert(ddcci_write(&mon, payload, 128) == -1);
	assert(ddcci_read(&mon, payload, 128) == -1);
	assert(ddcci_read(&mon, payload, (size_t)-1) == -1);
	assert(reads == 1 && writes == 2);
	/* This full 130-byte reply previously exceeded ddcci_read's stack buffer. */
	memset(device_reply, 0x23, sizeof(device_reply));
	device_reply[0] = 0x6e;
	device_reply[1] = 0xff;
	device_reply[129] = 0x50;
	for (i = 0; i < 129; i++)
		device_reply[129] ^= device_reply[i];
	assert(ddcci_read(&mon, payload, sizeof(payload)) == 127);
	assert_filled(payload, sizeof(payload), 0x23);
	device_reply[129] ^= 1;
	memset(payload, 0xa5, sizeof(payload));
	assert(ddcci_read(&mon, payload, sizeof(payload)) == -1);
	assert_filled(payload, sizeof(payload), 0xa5);
}

#endif

int main(void)
{
	test_build_frame();
	test_parse_frame();
	test_reply_parsers();
#ifdef HAVE_I2C_DEV
	test_transport_callers();
#endif
	return 0;
}
