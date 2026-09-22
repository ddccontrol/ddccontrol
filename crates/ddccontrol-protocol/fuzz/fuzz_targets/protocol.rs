#![no_main]

use ddccontrol_protocol::*;
use libfuzzer_sys::fuzz_target;

fuzz_target!(|input: &[u8]| {
    let Some((&address, bytes)) = input.split_first() else {
        return;
    };
    let control = bytes.first().copied().unwrap_or(0);
    let offset = u16::from_be_bytes([address, control]);
    let _ = parse_vcp_reply(bytes, control);
    let _ = parse_caps_reply(bytes, offset);
    for limit in [0, usize::from(address), MAX_PAYLOAD_LEN] {
        if let Ok(reply) = parse_frame(address, bytes, limit) {
            assert!(reply.payload.len() <= limit);
            assert!(reply.payload.len() + FRAME_OVERHEAD <= bytes.len());
            let _ = parse_vcp_reply(reply.payload, control);
            let _ = parse_caps_reply(reply.payload, offset);
        }
    }
    match build_frame(address, bytes) {
        Ok(frame) => {
            let built = frame.as_bytes();
            assert_eq!(built.len(), bytes.len() + FRAME_OVERHEAD);
            assert_eq!(&built[2..built.len() - 1], bytes);
            assert_eq!(built.iter().fold(address << 1, |xor, byte| xor ^ byte), 0);

            // Exercise valid replies as well as arbitrary input, so checksums
            // do not prevent the fuzzer reaching the payload parsers.
            let mut reply = [0; MAX_FRAME_LEN];
            let len = built.len();
            reply[..len].copy_from_slice(built);
            reply[0] = address << 1;
            reply[len - 1] = reply[..len - 1].iter().fold(0x50, |xor, byte| xor ^ byte);
            let parsed = parse_frame(address, &reply[..len], bytes.len()).unwrap();
            assert_eq!(parsed.payload, bytes);
            reply[len - 1] ^= 1;
            assert_eq!(
                parse_frame(address, &reply[..len], bytes.len()),
                Err(Error::InvalidChecksum)
            );
        }
        Err(_) => assert!(address > 0x7f || bytes.len() > MAX_PAYLOAD_LEN),
    }
});
