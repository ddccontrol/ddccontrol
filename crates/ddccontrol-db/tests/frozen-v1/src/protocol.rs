// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use ddccontrol_protocol::{self as protocol, Error, MAX_FRAME_LEN, MAX_PAYLOAD_LEN};
use libc::{c_int, c_uchar, c_uint, c_ushort, size_t};
use std::panic::catch_unwind;
use std::{ptr, slice};

fn status(error: Error) -> c_int {
    match error {
        Error::UnexpectedAddress => -2,
        Error::PayloadTooLong | Error::TruncatedFrame | Error::PayloadExceedsLimit => -3,
        Error::InvalidChecksum => -4,
        _ => -1,
    }
}

fn address(value: c_uint) -> Result<u8, c_int> {
    if value > 0x7f {
        return Err(-1);
    }
    Ok(value as u8)
}

#[no_mangle]
pub extern "C" fn ddccontrol_ddcci_frame_length(payload_len: size_t) -> c_int {
    protocol::frame_length(payload_len)
        .map(|len| len as c_int)
        .unwrap_or_else(status)
}

/// # Safety
/// Nonempty `payload` must be readable for `payload_len` bytes (at most 127).
/// `frame` must be writable for `frame_capacity` bytes without overlapping input.
/// A null payload is allowed only with zero length. Output is untouched on error.
#[no_mangle]
pub unsafe extern "C" fn ddccontrol_ddcci_build_frame(
    addr: c_uint,
    payload: *const c_uchar,
    payload_len: size_t,
    frame: *mut c_uchar,
    frame_capacity: size_t,
) -> c_int {
    catch_unwind(|| {
        let addr = address(addr)?;
        let len = protocol::frame_length(payload_len).map_err(status)?;
        if frame.is_null() || frame_capacity < len || (payload.is_null() && payload_len != 0) {
            return Err(-1);
        }
        let payload = if payload_len == 0 {
            &[]
        } else {
            slice::from_raw_parts(payload, payload_len)
        };
        let built = protocol::build_frame(addr, payload).map_err(status)?;
        ptr::copy_nonoverlapping(built.as_bytes().as_ptr(), frame, len);
        Ok(len as c_int)
    })
    .unwrap_or(Err(-1))
    .unwrap_or_else(|error| error)
}

/// # Safety
/// `frame` must be readable for `min(frame_len, 130)` bytes. `payload` must be
/// writable for `payload_capacity` bytes (or null with zero capacity), and an
/// optional `missing_length_flag` must point to a writable C int. All buffers
/// must be disjoint. Outputs are written only after complete validation.
#[no_mangle]
pub unsafe extern "C" fn ddccontrol_ddcci_parse_frame(
    addr: c_uint,
    frame: *const c_uchar,
    frame_len: size_t,
    payload: *mut c_uchar,
    payload_capacity: size_t,
    missing_length_flag: *mut c_int,
) -> c_int {
    catch_unwind(|| {
        let addr = address(addr)?;
        if frame.is_null() || (payload.is_null() && payload_capacity != 0) {
            return Err(-1);
        }
        let input = slice::from_raw_parts(frame, frame_len.min(MAX_FRAME_LEN));
        let reply = protocol::parse_frame(addr, input, payload_capacity).map_err(status)?;
        if !reply.payload.is_empty() {
            ptr::copy_nonoverlapping(reply.payload.as_ptr(), payload, reply.payload.len());
        }
        if !missing_length_flag.is_null() {
            ptr::write(missing_length_flag, c_int::from(reply.missing_length_flag));
        }
        Ok(reply.payload.len() as c_int)
    })
    .unwrap_or(Err(-1))
    .unwrap_or_else(|error| error)
}

/// # Safety
/// `payload` must be readable for `payload_len` bytes; lengths other than eight
/// are rejected before reading. Non-null value/maximum outputs must point to
/// writable, disjoint C unsigned shorts, separate from the input.
#[no_mangle]
pub unsafe extern "C" fn ddccontrol_ddcci_parse_vcp(
    payload: *const c_uchar,
    payload_len: size_t,
    control: c_uchar,
    value: *mut c_ushort,
    maximum: *mut c_ushort,
) -> c_int {
    catch_unwind(|| {
        if payload.is_null() || payload_len != 8 {
            return Err(-1);
        }
        let input = slice::from_raw_parts(payload, payload_len);
        let reply = protocol::parse_vcp_reply(input, control).map_err(|_| -1)?;
        if !value.is_null() {
            ptr::write(value, reply.value);
        }
        if !maximum.is_null() {
            ptr::write(maximum, reply.maximum);
        }
        Ok(c_int::from(reply.supported))
    })
    .unwrap_or(Err(-1))
    .unwrap_or_else(|error| error)
}

/// # Safety
/// `payload` must be readable for `payload_len` bytes. Lengths outside 3..=127
/// and offsets outside 0..=65535 are rejected before reading input.
#[no_mangle]
pub unsafe extern "C" fn ddccontrol_ddcci_parse_caps_reply(
    payload: *const c_uchar,
    payload_len: size_t,
    offset: c_uint,
) -> c_int {
    catch_unwind(|| {
        if payload.is_null() || !(3..=MAX_PAYLOAD_LEN).contains(&payload_len) {
            return Err(-1);
        }
        let offset = u16::try_from(offset).map_err(|_| -1)?;
        let input = slice::from_raw_parts(payload, payload_len);
        protocol::parse_caps_reply(input, offset)
            .map(|fragment| fragment.len() as c_int)
            .map_err(|_| -1)
    })
    .unwrap_or(Err(-1))
    .unwrap_or_else(|error| error)
}
