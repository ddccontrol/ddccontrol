/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "../ddcci.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

/* Build a minimal valid 128-byte EDID buffer with the given manufacturer bytes,
 * product code (little-endian), and video input definition byte (0x14). */
static void make_edid(unsigned char buf[128], unsigned char b8, unsigned char b9,
                      unsigned char prod_lo, unsigned char prod_hi,
                      unsigned char vid_input)
{
	memset(buf, 0, 128);
	/* Standard EDID header */
	buf[0] = 0x00;
	buf[1] = 0xff; buf[2] = 0xff; buf[3] = 0xff;
	buf[4] = 0xff; buf[5] = 0xff; buf[6] = 0xff;
	buf[7] = 0x00;
	/* Manufacturer ID bytes */
	buf[8] = b8;
	buf[9] = b9;
	/* Product code (little-endian) */
	buf[10] = prod_lo;
	buf[11] = prod_hi;
	/* Video input definition */
	buf[0x14] = vid_input;
}

static struct monitor make_mon(void)
{
	struct monitor mon;
	memset(&mon, 0, sizeof(mon));
	return mon;
}

/* ---- header validation -------------------------------------------------- */

static void test_valid_header_returns_zero(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
}

static void test_null_monitor_returns_error(void)
{
	unsigned char buf[128];
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(NULL, buf, 128) == -1);
}

static void test_null_buffer_returns_error(void)
{
	struct monitor mon = make_mon();
	CHECK(ddcci_parse_edid_buf(&mon, NULL, 128) == -1);
}

static void test_buffer_too_short_returns_error(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	/* Need at least DDCCI_EDID_MIN_PARSE_LEN bytes */
	CHECK(ddcci_parse_edid_buf(&mon, buf, DDCCI_EDID_MIN_PARSE_LEN - 1) == -1);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 0) == -1);
	CHECK(ddcci_parse_edid_buf(&mon, buf, -1) == -1);
}

static void test_exact_minimum_length_accepted(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, DDCCI_EDID_MIN_PARSE_LEN) == 0);
}

static void test_wrong_first_byte_rejected(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	buf[0] = 0x01;
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == -1);
}

static void test_wrong_last_header_byte_rejected(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	buf[7] = 0x01;
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == -1);
}

static void test_wrong_middle_header_byte_rejected(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	int i;
	for (i = 1; i <= 6; i++) {
		make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
		buf[i] = 0x00; /* should be 0xff */
		CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == -1);
	}
}

/* ---- PNP ID parsing ------------------------------------------------------ */

/* Samsung: manufacturer code SAM
 *   S = 19, A = 1, M = 13
 *   buf[8] = (19<<2) | (1>>3) = 76 = 0x4C
 *   buf[9] = ((1&7)<<5) | 13 = 32|13 = 45 = 0x2D
 */
static void test_pnpid_samsung(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0xa1, 0x01, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	/* product code little-endian: lo=0xa1 hi=0x01 -> printed as "01A1" */
	CHECK(strcmp(mon.pnpid, "SAM01A1") == 0);
}

/* Dell: manufacturer code DEL
 *   D=4, E=5, L=12
 *   buf[8] = (4<<2)|(5>>3) = 16|0 = 0x10
 *   buf[9] = ((5&7)<<5)|12 = 160|12 = 172 = 0xAC
 */
static void test_pnpid_dell(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x10, 0xac, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(strcmp(mon.pnpid, "DEL0000") == 0);
}

/* LG Electronics: manufacturer code GSM (as used in EDID)
 *   G=7, S=19, M=13
 *   buf[8] = (7<<2)|(19>>3) = 28|2 = 0x1E
 *   buf[9] = ((19&7)<<5)|13 = (3<<5)|13 = 96|13 = 109 = 0x6D
 */
static void test_pnpid_gsm(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x1e, 0x6d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(strcmp(mon.pnpid, "GSM0000") == 0);
}

static void test_pnpid_product_code_little_endian(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	/* SAM, product lo=0x34, hi=0x12 → printed as "1234" */
	make_edid(buf, 0x4c, 0x2d, 0x34, 0x12, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(strcmp(mon.pnpid, "SAM1234") == 0);
}

static void test_pnpid_max_product_code(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	/* product lo=0xFF, hi=0xFF → printed as "FFFF" */
	make_edid(buf, 0x4c, 0x2d, 0xff, 0xff, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(strcmp(mon.pnpid, "SAMFFFF") == 0);
}

static void test_pnpid_zero_product_code(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(strcmp(mon.pnpid, "SAM0000") == 0);
}

/* ---- digital / analog flag ----------------------------------------------- */

static void test_digital_flag_set_when_bit7_high(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x80);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x80);
}

static void test_digital_flag_clear_when_bit7_low(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x00);
}

static void test_digital_flag_ignores_lower_bits(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();
	/* Only bit 7 matters */
	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x7f);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x00);

	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0xff);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x80);
}

/* ---- idempotence / repeated calls --------------------------------------- */

static void test_successive_calls_overwrite_fields(void)
{
	unsigned char buf[128];
	struct monitor mon = make_mon();

	make_edid(buf, 0x4c, 0x2d, 0x00, 0x00, 0x80);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x80);
	CHECK(strcmp(mon.pnpid, "SAM0000") == 0);

	/* Second call with different data overwrites */
	make_edid(buf, 0x10, 0xac, 0xab, 0xcd, 0x00);
	CHECK(ddcci_parse_edid_buf(&mon, buf, 128) == 0);
	CHECK(mon.digital == 0x00);
	CHECK(strcmp(mon.pnpid, "DELCDAB") == 0);
}

int main(void)
{
	test_valid_header_returns_zero();
	test_null_monitor_returns_error();
	test_null_buffer_returns_error();
	test_buffer_too_short_returns_error();
	test_exact_minimum_length_accepted();
	test_wrong_first_byte_rejected();
	test_wrong_last_header_byte_rejected();
	test_wrong_middle_header_byte_rejected();
	test_pnpid_samsung();
	test_pnpid_dell();
	test_pnpid_gsm();
	test_pnpid_product_code_little_endian();
	test_pnpid_max_product_code();
	test_pnpid_zero_product_code();
	test_digital_flag_set_when_bit7_high();
	test_digital_flag_clear_when_bit7_low();
	test_digital_flag_ignores_lower_bits();
	test_successive_calls_overwrite_fields();
	return failures ? 1 : 0;
}
