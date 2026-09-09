// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use ddccontrol_protocol::*;

const VCP_FRAME: [u8; 11] = [0x6e, 0x88, 0x02, 0, 0x10, 0, 0x12, 0x34, 0xab, 0xcd, 0xe4];

fn reply_frame(payload: &[u8], length_flag: bool) -> Vec<u8> {
    assert!(payload.len() <= 127);
    let mut frame = vec![
        0x6e,
        payload.len() as u8 | if length_flag { 0x80 } else { 0 },
    ];
    frame.extend_from_slice(payload);
    frame.push(frame.iter().fold(0x50, |xor, byte| xor ^ byte));
    frame
}

#[test]
fn request_vectors_match_the_existing_wire_format() {
    let vectors: &[(&[u8], &[u8])] = &[
        (&[0x01, 0x10], &[0x51, 0x82, 0x01, 0x10, 0xac]),
        (
            &[0x03, 0x10, 0x12, 0x34],
            &[0x51, 0x84, 0x03, 0x10, 0x12, 0x34, 0x8e],
        ),
        (&[0xf3, 0x01, 0x02], &[0x51, 0x83, 0xf3, 0x01, 0x02, 0x4c]),
        (&[0x0c], &[0x51, 0x81, 0x0c, 0xb2]),
        (&[0xf7], &[0x51, 0x81, 0xf7, 0x49]),
        (&[], &[0x51, 0x80, 0xbf]),
    ];
    for (payload, expected) in vectors {
        assert_eq!(build_frame(0x37, payload).unwrap().as_bytes(), *expected);
    }
}

#[test]
fn request_length_and_address_boundaries() {
    for address in 0..=0x7f {
        for len in 0..=127 {
            let payload = vec![0xa5; len];
            let frame = build_frame(address, &payload).unwrap();
            let bytes = frame.as_bytes();
            assert_eq!(bytes.len(), len + 3);
            assert_eq!(frame_length(len), Ok(bytes.len()));
            assert_eq!(bytes[1], 0x80 | len as u8);
            assert_eq!(&bytes[2..len + 2], payload);
            assert_eq!(bytes.iter().fold(address << 1, |xor, byte| xor ^ byte), 0);
        }
    }
    for len in [128, 255, 256, usize::MAX] {
        assert_eq!(frame_length(len), Err(Error::PayloadTooLong));
    }
    assert_eq!(build_frame(0x37, &[0; 128]), Err(Error::PayloadTooLong));
    for address in 0x80..=0xff {
        assert_eq!(build_frame(address, &[]), Err(Error::InvalidAddress));
        assert_eq!(
            parse_frame(address, &VCP_FRAME, 8),
            Err(Error::InvalidAddress)
        );
    }
}

#[test]
fn valid_vcp_frame_and_big_endian_values() {
    let reply = parse_frame(0x37, &VCP_FRAME, 8).unwrap();
    assert!(!reply.missing_length_flag);
    assert_eq!(
        parse_vcp_reply(reply.payload, 0x10),
        Ok(VcpReply {
            supported: true,
            value: 0xabcd,
            maximum: 0x1234,
        })
    );
}

#[test]
fn every_truncation_of_a_reply_is_rejected() {
    for len in 0..VCP_FRAME.len() {
        assert_eq!(
            parse_frame(0x37, &VCP_FRAME[..len], 8),
            Err(Error::TruncatedFrame)
        );
    }
    let max = reply_frame(&[0x23; 127], true);
    assert_eq!(max.len(), 130);
    for len in 0..max.len() {
        assert_eq!(
            parse_frame(0x37, &max[..len], 127),
            Err(Error::TruncatedFrame)
        );
    }
    assert_eq!(parse_frame(0x37, &max, 127).unwrap().payload, &[0x23; 127]);
}

#[test]
fn length_header_is_bounded_by_received_bytes_and_output_limit() {
    for header in 0u8..=255 {
        let payload_len = usize::from(header & 0x7f);
        let payload = vec![0x23; payload_len];
        let frame = reply_frame(&payload, header & 0x80 != 0);
        for limit in 0..=127 {
            let result = parse_frame(0x37, &frame, limit);
            if limit < payload_len {
                assert_eq!(result, Err(Error::PayloadExceedsLimit));
            } else {
                assert_eq!(result.unwrap().payload, payload);
            }
        }
    }
    assert_eq!(
        parse_frame(0x37, &VCP_FRAME, usize::MAX)
            .unwrap()
            .payload
            .len(),
        8
    );
}

#[test]
fn trailing_i2c_padding_is_not_payload_or_checksum() {
    let mut frame = VCP_FRAME.to_vec();
    frame.extend_from_slice(&[0xff; 130]);
    assert_eq!(
        parse_frame(0x37, &frame, 8).unwrap().payload,
        &VCP_FRAME[2..10]
    );
    // A checksum in padding cannot repair a damaged declared checksum.
    frame[10] ^= 1;
    frame[11] ^= 1;
    assert_eq!(parse_frame(0x37, &frame, 8), Err(Error::InvalidChecksum));
}

#[test]
fn null_replies_and_missing_length_flag_are_accepted() {
    for frame in [&[0x6e, 0x80, 0xbe][..], &[0x6e, 0, 0x3e][..]] {
        let reply = parse_frame(0x37, frame, 0).unwrap();
        assert!(reply.payload.is_empty());
        assert_eq!(reply.missing_length_flag, frame[1] == 0);
        assert_eq!(
            parse_vcp_reply(reply.payload, 0x10),
            Err(Error::InvalidReplyLength)
        );
        assert_eq!(
            parse_caps_reply(reply.payload, 0),
            Err(Error::InvalidReplyLength)
        );
    }
    let frame = reply_frame(&VCP_FRAME[2..10], false);
    let reply = parse_frame(0x37, &frame, 8).unwrap();
    assert!(reply.missing_length_flag);
    assert_eq!(reply.payload, &VCP_FRAME[2..10]);
}

#[test]
fn wrong_address_and_every_single_bit_corruption_are_rejected() {
    assert_eq!(
        parse_frame(0x38, &VCP_FRAME, 8),
        Err(Error::UnexpectedAddress)
    );
    for index in 0..VCP_FRAME.len() {
        for bit in 0..8 {
            let mut frame = VCP_FRAME;
            frame[index] ^= 1 << bit;
            assert!(parse_frame(0x37, &frame, 8).is_err());
        }
    }
}

#[test]
fn vcp_reply_checks_length_opcode_and_requested_control() {
    let payload = &VCP_FRAME[2..10];
    for len in 0..8 {
        assert_eq!(
            parse_vcp_reply(&payload[..len], 0x10),
            Err(Error::InvalidReplyLength)
        );
    }
    assert_eq!(
        parse_vcp_reply(&[0; 9], 0x10),
        Err(Error::InvalidReplyLength)
    );
    assert_eq!(
        parse_vcp_reply(payload, 0x12),
        Err(Error::UnexpectedControl)
    );
    let mut wrong = payload.to_vec();
    wrong[0] = 0xe3;
    assert_eq!(parse_vcp_reply(&wrong, 0x10), Err(Error::UnexpectedCommand));
}

#[test]
fn unsupported_vcp_and_unknown_type_keep_legacy_semantics() {
    let mut payload = VCP_FRAME[2..10].to_vec();
    for result in 0..=255 {
        payload[1] = result;
        payload[3] = 0xff;
        let reply = parse_vcp_reply(&payload, 0x10).unwrap();
        assert_eq!(reply.supported, result == 0);
        assert_eq!(reply.value, 0xabcd);
        assert_eq!(reply.maximum, 0x1234);
    }
}

#[test]
fn capabilities_fragments_preserve_binary_data_and_check_end_markers() {
    let payload = [0xe3, 0x12, 0x34, b'A', 0, b')', 0xff];
    assert_eq!(parse_caps_reply(&payload, 0x1234).unwrap(), &payload[3..]);
    assert_eq!(
        parse_caps_reply(&payload, 0x3412),
        Err(Error::UnexpectedOffset)
    );
    for offset in [0u16, 0x1234, 0xffff] {
        let [high, low] = offset.to_be_bytes();
        assert!(parse_caps_reply(&[0xe3, high, low], offset)
            .unwrap()
            .is_empty());
        assert_eq!(
            parse_caps_reply(&[0, high, low], offset),
            Err(Error::UnexpectedCommand)
        );
        assert_eq!(
            parse_caps_reply(&[0xe3, high, low ^ 1], offset),
            Err(Error::UnexpectedOffset)
        );
    }
    for len in 0..3 {
        assert_eq!(
            parse_caps_reply(&payload[..len], 0),
            Err(Error::InvalidReplyLength)
        );
    }
    let mut max = [0x41; 127];
    max[..3].copy_from_slice(&[0xe3, 0xff, 0xff]);
    assert_eq!(parse_caps_reply(&max, 0xffff).unwrap().len(), 124);
    assert_eq!(
        parse_caps_reply(&[0; 128], 0),
        Err(Error::InvalidReplyLength)
    );
}
