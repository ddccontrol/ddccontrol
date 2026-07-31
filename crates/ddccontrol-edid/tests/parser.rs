// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use ddccontrol_edid::{parse, ParseError, EDID_BLOCK_LEN, EDID_MIN_PARSE_LEN};

const EDID_HEADER: [u8; 8] = [0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00];
const DESCRIPTOR_START: usize = 0x36;
const DESCRIPTOR_LEN: usize = 18;
const DESCRIPTOR_TEXT_LEN: usize = 13;

fn make_edid() -> [u8; EDID_BLOCK_LEN] {
    let mut edid = [0; EDID_BLOCK_LEN];
    edid[..EDID_HEADER.len()].copy_from_slice(&EDID_HEADER);
    edid[8] = 0x4c;
    edid[9] = 0x2d;
    edid
}

fn set_descriptor(edid: &mut [u8; EDID_BLOCK_LEN], index: usize, kind: u8, text: &[u8]) {
    let start = DESCRIPTOR_START + index * DESCRIPTOR_LEN;
    edid[start..start + DESCRIPTOR_LEN].fill(0);
    edid[start + 3] = kind;
    let text_len = text.len().min(DESCRIPTOR_TEXT_LEN);
    edid[start + 5..start + 5 + text_len].copy_from_slice(&text[..text_len]);
    if text_len < DESCRIPTOR_TEXT_LEN {
        edid[start + 5 + text_len] = b'\n';
    }
}

#[test]
fn parses_legacy_fields_and_little_endian_numbers() {
    let mut input = make_edid();
    input[10] = 0x34;
    input[11] = 0x12;
    input[0x0c..=0x0f].copy_from_slice(&[0x78, 0x56, 0x34, 0x12]);
    input[0x10] = 22;
    input[0x11] = 30;
    input[0x12] = 1;
    input[0x13] = 4;
    input[0x14] = 0xff;
    input[0x15] = 60;
    input[0x16] = 34;

    let parsed = parse(&input).unwrap();
    assert_eq!(parsed.pnp_id(), "SAM1234");
    assert!(parsed.is_digital_input());
    assert_eq!(parsed.raw(), input);
    assert_eq!(parsed.info().serial_number, 0x1234_5678);
    assert_eq!(parsed.info().manufacture_week, 22);
    assert_eq!(parsed.info().manufacture_year, 2020);
    assert_eq!(parsed.info().version, 1);
    assert_eq!(parsed.info().revision, 4);
    assert_eq!(parsed.info().max_width_cm, 60);
    assert_eq!(parsed.info().max_height_cm, 34);
}

#[test]
fn accepts_and_preserves_the_minimum_partial_block() {
    let input = make_edid();
    let parsed = parse(&input[..EDID_MIN_PARSE_LEN]).unwrap();

    assert_eq!(parsed.raw(), &input[..EDID_MIN_PARSE_LEN]);
    assert!(parsed.info().monitor_name.is_empty());
    assert!(parsed.info().serial_ascii.is_empty());
}

#[test]
fn rejects_short_buffers_and_invalid_headers() {
    let mut input = make_edid();
    assert_eq!(
        parse(&input[..EDID_MIN_PARSE_LEN - 1]),
        Err(ParseError::TooShort {
            actual: EDID_MIN_PARSE_LEN - 1
        })
    );

    input[4] = 0;
    assert_eq!(parse(&input), Err(ParseError::InvalidHeader));
}

#[test]
fn parses_and_normalizes_descriptor_text() {
    let mut input = make_edid();
    set_descriptor(&mut input, 0, 0xfc, b"Panel\x01 27   ");
    set_descriptor(&mut input, 1, 0xff, b"ABC123\rignored");

    let parsed = parse(&input).unwrap();
    assert_eq!(parsed.info().monitor_name, "Panel  27");
    assert_eq!(parsed.info().serial_ascii, "ABC123");
}

#[test]
fn ignores_non_text_descriptors_and_uses_the_last_matching_one() {
    let mut input = make_edid();
    set_descriptor(&mut input, 0, 0xfc, b"First");
    set_descriptor(&mut input, 1, 0xfd, b"Not a name");
    set_descriptor(&mut input, 2, 0xfc, b"Last");
    input[DESCRIPTOR_START + 3 * DESCRIPTOR_LEN] = 1;
    input[DESCRIPTOR_START + 3 * DESCRIPTOR_LEN + 3] = 0xff;

    let parsed = parse(&input).unwrap();
    assert_eq!(parsed.info().monitor_name, "Last");
    assert!(parsed.info().serial_ascii.is_empty());
}

#[test]
fn limits_the_stored_base_block_to_128_bytes() {
    let mut input = make_edid().to_vec();
    input.extend_from_slice(&[1, 2, 3, 4]);

    let parsed = parse(&input).unwrap();
    assert_eq!(parsed.raw().len(), EDID_BLOCK_LEN);
    assert_eq!(parsed.raw(), &input[..EDID_BLOCK_LEN]);
}
