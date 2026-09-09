// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! DDC/CI wire format, independent of I2C, timing, retries and monitor state.

#![forbid(unsafe_code)]

pub const MAX_PAYLOAD_LEN: usize = 127;
pub const FRAME_OVERHEAD: usize = 3;
pub const MAX_FRAME_LEN: usize = MAX_PAYLOAD_LEN + FRAME_OVERHEAD;

const HOST_ADDRESS: u8 = 0x51;
const LENGTH_FLAG: u8 = 0x80;
const REPLY_CHECKSUM_SEED: u8 = 0x50;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Error {
    InvalidAddress,
    PayloadTooLong,
    TruncatedFrame,
    UnexpectedAddress,
    PayloadExceedsLimit,
    InvalidChecksum,
    InvalidReplyLength,
    UnexpectedCommand,
    UnexpectedControl,
    UnexpectedOffset,
}

/// A complete request, excluding the I2C destination address.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Frame {
    bytes: [u8; MAX_FRAME_LEN],
    len: usize,
}

impl Frame {
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes[..self.len]
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Reply<'a> {
    pub payload: &'a [u8],
    /// Some Fujitsu Siemens and NEC monitors omit bit 7 of the length byte.
    pub missing_length_flag: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct VcpReply {
    pub supported: bool,
    pub value: u16,
    pub maximum: u16,
}

/// Check the seven-bit payload length before adding header and checksum bytes.
pub fn frame_length(payload_len: usize) -> Result<usize, Error> {
    if payload_len > MAX_PAYLOAD_LEN {
        return Err(Error::PayloadTooLong);
    }
    Ok(payload_len + FRAME_OVERHEAD)
}

fn wire_address(address: u8) -> Result<u8, Error> {
    if address > 0x7f {
        return Err(Error::InvalidAddress);
    }
    Ok(address << 1)
}

/// Build a request using the seven-bit I2C address as the checksum seed.
/// Empty payloads are valid; payloads over 127 bytes are rejected.
pub fn build_frame(address: u8, payload: &[u8]) -> Result<Frame, Error> {
    let seed = wire_address(address)?;
    let len = frame_length(payload.len())?;
    let mut bytes = [0; MAX_FRAME_LEN];
    bytes[0] = HOST_ADDRESS;
    bytes[1] = LENGTH_FLAG | payload.len() as u8;
    bytes[2..len - 1].copy_from_slice(payload);
    bytes[len - 1] = checksum(seed, &bytes[..len - 1]);
    Ok(Frame { bytes, len })
}

/// Validate a reply against the actual received bytes and caller's payload limit.
///
/// Fixed-size I2C reads may contain padding after the declared checksum. Only
/// the declared frame is checksummed and returned. A valid null reply returns
/// an empty payload. A missing length flag is accepted for legacy monitors.
pub fn parse_frame(address: u8, input: &[u8], payload_limit: usize) -> Result<Reply<'_>, Error> {
    let expected_address = wire_address(address)?;
    if input.len() < FRAME_OVERHEAD {
        return Err(Error::TruncatedFrame);
    }
    if input[0] != expected_address {
        return Err(Error::UnexpectedAddress);
    }
    let payload_len = usize::from(input[1] & !LENGTH_FLAG);
    if payload_len > payload_limit {
        return Err(Error::PayloadExceedsLimit);
    }
    let len = payload_len + FRAME_OVERHEAD;
    if input.len() < len {
        return Err(Error::TruncatedFrame);
    }
    if checksum(REPLY_CHECKSUM_SEED, &input[..len]) != 0 {
        return Err(Error::InvalidChecksum);
    }
    Ok(Reply {
        payload: &input[2..len - 1],
        missing_length_flag: input[1] & LENGTH_FLAG == 0,
    })
}

/// Parse the eight-byte Get VCP reply payload, checking the requested control.
/// As in the C API, any nonzero result byte means unsupported; the type byte
/// is ignored and value/maximum are still returned for unsupported controls.
pub fn parse_vcp_reply(payload: &[u8], control: u8) -> Result<VcpReply, Error> {
    if payload.len() != 8 {
        return Err(Error::InvalidReplyLength);
    }
    if payload[0] != 0x02 {
        return Err(Error::UnexpectedCommand);
    }
    if payload[2] != control {
        return Err(Error::UnexpectedControl);
    }
    Ok(VcpReply {
        supported: payload[1] == 0,
        maximum: u16::from_be_bytes([payload[4], payload[5]]),
        value: u16::from_be_bytes([payload[6], payload[7]]),
    })
}

/// Parse a capabilities reply payload at the requested offset. The fragment
/// remains binary data, including embedded NULs. An empty fragment is the end
/// marker only after the command and offset have been validated.
pub fn parse_caps_reply(payload: &[u8], offset: u16) -> Result<&[u8], Error> {
    if !(3..=MAX_PAYLOAD_LEN).contains(&payload.len()) {
        return Err(Error::InvalidReplyLength);
    }
    if payload[0] != 0xe3 {
        return Err(Error::UnexpectedCommand);
    }
    if u16::from_be_bytes([payload[1], payload[2]]) != offset {
        return Err(Error::UnexpectedOffset);
    }
    Ok(&payload[3..])
}

fn checksum(seed: u8, bytes: &[u8]) -> u8 {
    bytes.iter().fold(seed, |xor, byte| xor ^ byte)
}
