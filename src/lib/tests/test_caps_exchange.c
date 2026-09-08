/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "config.h"

#ifdef HAVE_I2C_DEV

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../ddcci.h"
#include "../i2c-dev.h"

enum reply_fault {
	NO_FAULT, READ_ERROR, SHORT_REPLY, WRONG_COMMAND, WRONG_OFFSET, BAD_CHECKSUM
};

static const unsigned char *input;
static size_t input_len, chunk_size, requested_offset, previous_offset;
static size_t fault_offset;
static enum reply_fault fault;
static int faults_left, reads, writes, allocation_calls, fail_allocation;
static void *raw_allocation;

static void *caps_test_malloc(size_t size)
{
	assert(raw_allocation == NULL);
	if (++allocation_calls == fail_allocation)
		return NULL;
	raw_allocation = malloc(size);
	assert(raw_allocation != NULL);
	return raw_allocation;
}

static void *caps_test_realloc(void *ptr, size_t size)
{
	assert(ptr == raw_allocation);
	assert(size <= 0x10000);
	if (++allocation_calls == fail_allocation)
		return NULL;
	raw_allocation = realloc(ptr, size);
	assert(raw_allocation != NULL);
	return raw_allocation;
}

static void caps_test_free(void *ptr)
{
	if (ptr) {
		assert(ptr == raw_allocation);
		raw_allocation = NULL;
		free(ptr);
	}
}

static int caps_test_usleep(useconds_t usec)
{
	(void)usec;
	return 0;
}

/* Exercise real DDC/CI framing and CAPS collection using an in-memory I2C
 * device. No hardware, linker wrapping, or architecture-specific ABI is used. */
static int caps_test_ioctl(int fd, unsigned long request, ...)
{
	va_list args;
	struct i2c_rdwr_ioctl_data *transfer;
	struct i2c_msg *message;
	unsigned char *frame, checksum;
	size_t count, payload_len, i;
	enum reply_fault current_fault = NO_FAULT;

	assert(fd == 123);
	assert(request == I2C_RDWR);
	va_start(args, request);
	transfer = va_arg(args, struct i2c_rdwr_ioctl_data *);
	va_end(args);
	assert(transfer->nmsgs == 1);
	message = transfer->msgs;
	frame = message->buf;
	if (!(message->flags & I2C_M_RD)) {
		assert(message->len == 6);
		assert(frame[0] == 0x51 && frame[1] == 0x83 && frame[2] == 0xf3);
		requested_offset = (size_t)frame[3] * 256 + frame[4];
		assert(requested_offset >= previous_offset);
		assert(requested_offset <= input_len);
		previous_offset = requested_offset;
		writes++;
	} else {
		reads++;
		assert(message->len == 67);
		if (requested_offset == fault_offset && faults_left > 0) {
			current_fault = fault;
			faults_left--;
		}
		if (current_fault == READ_ERROR) {
			errno = EIO;
			return -1;
		}

		count = input_len - requested_offset;
		if (count > chunk_size)
			count = chunk_size;
		payload_len = current_fault == SHORT_REPLY ? 2 : count + 3;
		memset(frame, 0, message->len);
		frame[0] = 0x6e;
		frame[1] = 0x80 | payload_len;
		frame[2] = current_fault == WRONG_COMMAND ? 0 : 0xe3;
		frame[3] = requested_offset >> 8;
		frame[4] = requested_offset & 0xff;
		if (current_fault == WRONG_OFFSET)
			frame[4] ^= 1;
		if (count)
			memcpy(frame + 5, input + requested_offset, count);
		checksum = 0x50;
		for (i = 0; i < payload_len + 2; i++)
			checksum ^= frame[i];
		frame[payload_len + 2] = checksum;
		if (current_fault == BAD_CHECKSUM)
			frame[payload_len + 2] ^= 1;
	}

#ifdef __FreeBSD__
	return 0;
#else
	return 1;
#endif
}

/* Compile the production implementation with local hooks, keeping allocation
 * failure injection out of the library and out of the Rust/C allocator ABI. */
#define malloc caps_test_malloc
#define realloc caps_test_realloc
#define free caps_test_free
#define ioctl caps_test_ioctl
#define usleep caps_test_usleep
#include "../ddcci.c"
#undef malloc
#undef realloc
#undef free
#undef ioctl
#undef usleep

static struct monitor prepare_exchange(const void *data, size_t length)
{
	struct monitor mon;
	assert(raw_allocation == NULL);
	memset(&mon, 0, sizeof(mon));
	mon.type = dev;
	mon.addr = 0x37;
	mon.fd = 123;
	mon.probing = 1;
	input = data;
	input_len = length;
	chunk_size = 5;
	requested_offset = previous_offset = 0;
	fault_offset = 0;
	fault = NO_FAULT;
	faults_left = reads = writes = allocation_calls = fail_allocation = 0;
	return mon;
}

static void release_caps(struct monitor *mon)
{
	int i;
	caps_test_free(mon->caps.raw_caps);
	mon->caps.raw_caps = NULL;
	for (i = 0; i < 256; i++) {
		if (mon->caps.vcp[i]) {
			free(mon->caps.vcp[i]->values);
			free(mon->caps.vcp[i]);
			mon->caps.vcp[i] = NULL;
		}
	}
}

static void assert_exchange_failed(struct monitor *mon)
{
	assert(ddcci_caps(mon) == -1);
	assert(mon->caps.raw_caps == NULL);
	assert(raw_allocation == NULL);
	release_caps(mon);
}

static void test_binary_payloads_and_chunk_boundaries(void)
{
	static const char expected[] = "(edid bin(4(####))vdif bin(0x4(####))bin(00() )vcp(10 12))";
	/* The first payload is four bytes: A, NUL, ')', ')'. */
	static const char binary[] = "(edid bin(4(A\0))))vdif bin(0x4(bin())bin(00() )vcp(10 12))";
	size_t chunk;

	for (chunk = 1; chunk <= 61; chunk++) {
		struct monitor mon = prepare_exchange(binary, sizeof(binary) - 1);
		chunk_size = chunk;
		assert(ddcci_caps(&mon) == (int)sizeof(binary) - 1);
		assert(strcmp(mon.caps.raw_caps, expected) == 0);
		assert(mon.caps.vcp[0x10] != NULL && mon.caps.vcp[0x12] != NULL);
		release_caps(&mon);
	}
}

static void test_invalid_binary_lengths(void)
{
	const char *invalid[] = {
		"(bin(1000000(A))vcp(10))", "(bin(-1(A))vcp(10))",
		"(bin(184467440737095516160(A))vcp(10))",
		"(bin(0x100000000(A))vcp(10))", "(bin((A))vcp(10))",
		"(bin(nope(A))vcp(10))", "(bin(1A))vcp(10))",
		"(bin(4(AB", "(bin(1(A)", "(bin(1(A", "(bin("
	};
	size_t i;
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		struct monitor mon = prepare_exchange(invalid[i], strlen(invalid[i]));
		assert_exchange_failed(&mon);
	}
}

static void test_allocation_failures(void)
{
	static const char data[] = "(vcp(10 12))";
	int failure;
	/* Initial malloc and every subsequent realloc must clean up on failure. */
	for (failure = 1; failure <= 4; failure++) {
		struct monitor mon = prepare_exchange(data, sizeof(data) - 1);
		fail_allocation = failure;
		assert_exchange_failed(&mon);
		assert(allocation_calls == failure);
		if (failure == 1)
			assert(reads == 0 && writes == 0);
	}
}

static void test_reply_failures_and_retries(void)
{
	static const char data[] = "(vcp(10))";
	enum reply_fault next;
	for (next = READ_ERROR; next <= BAD_CHECKSUM; next++) {
		struct monitor mon = prepare_exchange(data, sizeof(data) - 1);
		fault = next;
		fault_offset = 5;
		faults_left = 3;
		assert_exchange_failed(&mon);
		assert(reads == 4 && writes == 4 && faults_left == 0);

		mon = prepare_exchange(data, sizeof(data) - 1);
		fault = next;
		faults_left = 2;
		assert(ddcci_caps(&mon) == (int)sizeof(data) - 1);
		assert(mon.caps.vcp[0x10] != NULL);
		assert(reads == 5 && writes == 5);
		release_caps(&mon);
	}
	/* A three-byte reply with the wrong offset/command is not an end marker. */
	for (next = WRONG_COMMAND; next <= WRONG_OFFSET; next++) {
		struct monitor mon = prepare_exchange(data, sizeof(data) - 1);
		fault = next;
		fault_offset = sizeof(data) - 1;
		faults_left = 3;
		assert_exchange_failed(&mon);
		assert(reads == 5 && faults_left == 0);
	}
}

static void test_offset_limit(void)
{
	size_t length;
	for (length = 0xffff; length <= 0x10000; length++) {
		char *data = malloc(length);
		struct monitor mon;
		assert(data != NULL);
		memset(data, ' ', length);
		memcpy(data, "(vcp(10))", 9);
		mon = prepare_exchange(data, length);
		chunk_size = 61;
		if (length == 0xffff) {
			assert(ddcci_caps(&mon) == (int)length);
			assert(requested_offset == 0xffff);
			assert(mon.caps.vcp[0x10] != NULL);
			release_caps(&mon);
		} else {
			assert_exchange_failed(&mon);
		}
		free(data);
	}
}

static void test_repeated_caps_reads_release_previous_buffer(void)
{
	static const char data[] = "(vcp(10))";
	struct monitor mon = prepare_exchange(data, sizeof(data) - 1);
	assert(ddcci_caps(&mon) == (int)sizeof(data) - 1);
	previous_offset = 0;
	assert(ddcci_caps(&mon) == (int)sizeof(data) - 1);
	assert(mon.caps.vcp[0x10] != NULL);
	previous_offset = 0;
	fail_allocation = allocation_calls + 2;
	assert_exchange_failed(&mon);
}

int main(void)
{
	static const char malformed[] = "(vcp(10";
	struct monitor mon;
	test_binary_payloads_and_chunk_boundaries();
	test_invalid_binary_lengths();
	test_allocation_failures();
	test_reply_failures_and_retries();
	test_offset_limit();
	test_repeated_caps_reads_release_previous_buffer();
	mon = prepare_exchange(malformed, sizeof(malformed) - 1);
	assert_exchange_failed(&mon);
	return 0;
}

#else

int main(void)
{
	return 77; /* Automake skip: the I2C backend is disabled. */
}

#endif
