// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Direct Linux I2C access. The existing protocol and EDID crates own all wire
//! decoding; this module supplies device discovery, timing and read retries.
//! Only EDID offsets, Get Capabilities and Get VCP requests are written.

use ddccontrol_edid::{self as edid, Edid};
use ddccontrol_protocol as protocol;
use std::fs::{self, File, OpenOptions};
use std::io;
use std::time::{Duration, Instant};

const DDC_ADDRESS: u16 = 0x37;
const EDID_ADDRESS: u16 = 0x50;
const DELAY: Duration = Duration::from_millis(45);
const PERMISSION_HELP: &str =
    "Grant read/write access to /dev/i2c-* or run ddccontrol-scanmonitor with sudo.";

#[derive(Debug, Eq, PartialEq)]
pub struct Monitor {
    pub device: String,
    pub name: String,
}

pub struct OpenedMonitor {
    pub pnp_id: String,
    pub capabilities: String,
    pub name: Option<String>,
    transport: Device,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Reading {
    pub current: u16,
    pub maximum: u16,
}

impl OpenedMonitor {
    pub fn read(&mut self, code: u8) -> Result<Option<Reading>, String> {
        read_control(&mut self.transport, code)
    }
}

/// List EDID-identifiable monitors without reading their controls or changing
/// settings. Internal panels may expose EDID without supporting DDC/CI.
pub fn list() -> Result<Vec<Monitor>, String> {
    require_linux()?;
    let mut devices = fs::read_dir("/dev")
        .map_err(|error| format!("Cannot list /dev: {error}"))?
        .filter_map(Result::ok)
        .filter_map(|entry| {
            let name = entry.file_name();
            let number = device_number(name.to_str()?)?;
            Some((number, entry.path()))
        })
        .collect::<Vec<_>>();
    devices.sort_by_key(|(number, _)| *number);
    if devices.is_empty() {
        return Err(
            "No /dev/i2c-* devices found; load the i2c-dev kernel module (sudo modprobe i2c-dev)."
                .into(),
        );
    }

    let mut monitors = Vec::new();
    let mut denied = 0;
    for (_, path) in devices {
        let file = match OpenOptions::new().read(true).write(true).open(&path) {
            Ok(file) => file,
            Err(error) => {
                if error.kind() == io::ErrorKind::PermissionDenied {
                    denied += 1;
                }
                continue;
            }
        };
        let mut transport = Device::new(file);
        if let Ok(info) = read_edid(&mut transport) {
            monitors.push(Monitor {
                device: format!("dev:{}", path.display()),
                name: edid_name(&info).unwrap_or_else(|| info.pnp_id().to_owned()),
            });
        }
    }
    if denied != 0 {
        let message = format!("Permission denied for {denied} I2C device(s). {PERMISSION_HELP}");
        if monitors.is_empty() {
            return Err(message);
        }
        eprintln!("{message}");
    }
    Ok(monitors)
}

pub fn open(device: &str) -> Result<OpenedMonitor, String> {
    require_linux()?;
    let path = device
        .strip_prefix("dev:/dev/")
        .filter(|name| device_number(name).is_some())
        .ok_or_else(|| "Expected a device such as dev:/dev/i2c-4".to_string())?;
    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .open(format!("/dev/{path}"))
        .map_err(|error| format!("Cannot open {device}: {error}. {PERMISSION_HELP}"))?;
    let mut transport = Device::new(file);
    let info = read_edid(&mut transport)
        .map_err(|error| format!("Cannot read monitor identification from {device}: {error}"))?;
    // Some monitors provide readable controls despite a broken caps exchange.
    let capabilities = read_capabilities(&mut transport).unwrap_or_else(|error| {
        eprintln!(
            "Cannot read capabilities from {device}: {error}; trying known control addresses."
        );
        String::new()
    });
    let name = edid_name(&info).or_else(|| capability_model(&capabilities));
    Ok(OpenedMonitor {
        pnp_id: info.pnp_id().to_owned(),
        capabilities,
        name,
        transport,
    })
}

fn require_linux() -> Result<(), String> {
    if cfg!(target_os = "linux") {
        Ok(())
    } else {
        Err("Direct monitor scanning currently requires Linux I2C devices.".into())
    }
}

fn device_number(name: &str) -> Option<u32> {
    let number = name.strip_prefix("i2c-")?;
    if number.is_empty() || !number.bytes().all(|byte| byte.is_ascii_digit()) {
        return None;
    }
    number.parse().ok()
}

fn edid_name(info: &Edid) -> Option<String> {
    let name = info.info().monitor_name.trim();
    (!name.is_empty()).then(|| name.to_owned())
}

trait Transport {
    fn transfer(&mut self, address: u16, read: bool, bytes: &mut [u8]) -> io::Result<()>;
}

struct Device {
    file: File,
    last_write: Option<Instant>,
}

impl Device {
    fn new(file: File) -> Self {
        Self {
            file,
            last_write: None,
        }
    }
}

impl Transport for Device {
    fn transfer(&mut self, address: u16, read: bool, bytes: &mut [u8]) -> io::Result<()> {
        if let Some(last_write) = self.last_write {
            std::thread::sleep(DELAY.saturating_sub(last_write.elapsed()));
        }
        let result = transfer(&self.file, address, read, bytes);
        if !read {
            self.last_write = Some(Instant::now());
        }
        result
    }
}

#[cfg(target_os = "linux")]
fn transfer(file: &File, address: u16, read: bool, bytes: &mut [u8]) -> io::Result<()> {
    use libc::{c_uchar, c_uint, c_ushort};
    use std::os::fd::AsRawFd;

    // Linux uapi/linux/i2c.h and i2c-dev.h. C layout includes pointer padding
    // on 64-bit targets; no host byte order assumptions enter the wire data.
    #[repr(C)]
    struct I2cMessage {
        address: c_ushort,
        flags: c_ushort,
        length: c_ushort,
        buffer: *mut c_uchar,
    }
    #[repr(C)]
    struct I2cTransfer {
        messages: *mut I2cMessage,
        count: c_uint,
    }
    let mut message = I2cMessage {
        address,
        flags: u16::from(read), // I2C_M_RD == 1
        length: bytes
            .len()
            .try_into()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "I2C message is too long"))?,
        buffer: bytes.as_mut_ptr(),
    };
    let mut request = I2cTransfer {
        messages: &mut message,
        count: 1,
    };
    // SAFETY: The request, message and mutable byte buffer remain alive and
    // exclusively borrowed until this synchronous I2C_RDWR ioctl completes.
    let result = unsafe { libc::ioctl(file.as_raw_fd(), 0x0707, &mut request) };
    match result {
        1 => Ok(()), // The kernel returns message count, not byte count.
        -1 => Err(io::Error::last_os_error()),
        _ => Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "Incomplete I2C transfer",
        )),
    }
}

#[cfg(not(target_os = "linux"))]
fn transfer(_: &File, _: u16, _: bool, _: &mut [u8]) -> io::Result<()> {
    Err(io::Error::new(
        io::ErrorKind::Unsupported,
        "Linux I2C is required",
    ))
}

fn retry<T>(mut operation: impl FnMut() -> Result<T, String>) -> Result<T, String> {
    let mut last_error = String::new();
    for _ in 0..3 {
        match operation() {
            Ok(value) => return Ok(value),
            Err(error) => last_error = error,
        }
    }
    Err(last_error)
}

fn read_edid(transport: &mut impl Transport) -> Result<Edid, String> {
    retry(|| {
        transport
            .transfer(EDID_ADDRESS, false, &mut [0])
            .map_err(|error| error.to_string())?;
        let mut bytes = [0; edid::EDID_BLOCK_LEN];
        transport
            .transfer(EDID_ADDRESS, true, &mut bytes)
            .map_err(|error| error.to_string())?;
        let info = edid::parse(&bytes).map_err(|error| error.to_string())?;
        if !edid::is_valid_pnp_id(info.pnp_id()) {
            return Err("EDID contains an invalid monitor Plug and Play ID".into());
        }
        Ok(info)
    })
}

fn exchange(
    transport: &mut impl Transport,
    payload: &[u8],
    reply_size: usize,
) -> Result<Vec<u8>, String> {
    let mut request = protocol::build_frame(DDC_ADDRESS as u8, payload)
        .map_err(|error| format!("Invalid DDC request: {error:?}"))?
        .as_bytes()
        .to_vec();
    transport
        .transfer(DDC_ADDRESS, false, &mut request)
        .map_err(|error| error.to_string())?;
    let mut response = vec![0; reply_size + protocol::FRAME_OVERHEAD];
    transport
        .transfer(DDC_ADDRESS, true, &mut response)
        .map_err(|error| error.to_string())?;
    let frame = protocol::parse_frame(DDC_ADDRESS as u8, &response, reply_size)
        .map_err(|error| format!("Invalid DDC reply: {error:?}"))?;
    Ok(frame.payload.to_vec())
}

fn read_control(transport: &mut impl Transport, code: u8) -> Result<Option<Reading>, String> {
    retry(|| {
        let reply = exchange(transport, &[0x01, code], 8)?;
        let reply = protocol::parse_vcp_reply(&reply, code)
            .map_err(|error| format!("Invalid control reply: {error:?}"))?;
        Ok(reply.supported.then_some(Reading {
            current: reply.value,
            maximum: reply.maximum,
        }))
    })
}

fn read_capabilities(transport: &mut impl Transport) -> Result<String, String> {
    let mut bytes = Vec::new();
    loop {
        let offset = bytes.len() as u16;
        let fragment = retry(|| {
            let [high, low] = offset.to_be_bytes();
            let reply = exchange(transport, &[0xf3, high, low], 64)?;
            protocol::parse_caps_reply(&reply, offset)
                .map(<[u8]>::to_vec)
                .map_err(|error| format!("Invalid capabilities fragment: {error:?}"))
        })?;
        if fragment.is_empty() {
            return String::from_utf8(bytes)
                .map_err(|error| format!("Capabilities are not UTF-8 text: {error}"));
        }
        if bytes.len() + fragment.len() > usize::from(u16::MAX) {
            return Err("Capabilities exceed the 65535-byte DDC limit".into());
        }
        bytes.extend_from_slice(&fragment);
    }
}

fn capability_model(capabilities: &str) -> Option<String> {
    for (index, _) in capabilities.match_indices("model(") {
        if index != 0
            && !matches!(capabilities.as_bytes()[index - 1], b'(' | b')')
            && !capabilities.as_bytes()[index - 1].is_ascii_whitespace()
        {
            continue;
        }
        let rest = &capabilities[index + "model(".len()..];
        let model = rest.split_once(')')?.0.trim();
        if !model.is_empty() && !model.contains('(') {
            return Some(model.to_owned());
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::VecDeque;

    #[derive(Default)]
    struct MockTransport {
        replies: VecDeque<io::Result<Vec<u8>>>,
        writes: Vec<(u16, Vec<u8>)>,
    }

    impl Transport for MockTransport {
        fn transfer(&mut self, address: u16, read: bool, bytes: &mut [u8]) -> io::Result<()> {
            if read {
                let reply = self.replies.pop_front().expect("unexpected read")?;
                assert!(reply.len() <= bytes.len());
                bytes[..reply.len()].copy_from_slice(&reply);
            } else {
                // An independent allowlist checks every request, including retries.
                match address {
                    EDID_ADDRESS => assert_eq!(bytes, [0]),
                    DDC_ADDRESS => assert!(
                        matches!(bytes[2], 0x01 | 0xf3),
                        "not a read request: {bytes:?}"
                    ),
                    _ => panic!("unexpected address {address}"),
                }
                self.writes.push((address, bytes.to_vec()));
            }
            Ok(())
        }
    }

    fn reply(payload: &[u8]) -> Vec<u8> {
        let mut frame = vec![0x6e, 0x80 | payload.len() as u8];
        frame.extend_from_slice(payload);
        frame.push(frame.iter().fold(0x50, |checksum, byte| checksum ^ byte));
        frame
    }

    fn caps_reply(offset: u16, bytes: &[u8]) -> Vec<u8> {
        let [high, low] = offset.to_be_bytes();
        let mut payload = vec![0xe3, high, low];
        payload.extend_from_slice(bytes);
        reply(&payload)
    }

    #[test]
    fn edid_retries_bad_data_and_retains_name_and_pnp_id() {
        let mut bytes = [0; 128];
        bytes[..8].copy_from_slice(&[0, 255, 255, 255, 255, 255, 255, 0]);
        bytes[8..12].copy_from_slice(&[0x10, 0xac, 0x34, 0x12]);
        bytes[0x39] = 0xfc;
        bytes[0x3b..0x48].copy_from_slice(b"Test display\n");
        let mut transport = MockTransport::default();
        transport
            .replies
            .extend([Ok(vec![0; 128]), Ok(bytes.to_vec())]);
        let info = read_edid(&mut transport).unwrap();
        assert_eq!(info.pnp_id(), "DEL1234");
        assert_eq!(edid_name(&info).as_deref(), Some("Test display"));
        assert_eq!(transport.writes.len(), 2);
    }

    #[test]
    fn invalid_edid_manufacturer_cannot_become_an_output_filename() {
        let mut bytes = vec![0; 128];
        bytes[..8].copy_from_slice(&[0, 255, 255, 255, 255, 255, 255, 0]);
        let mut transport = MockTransport::default();
        for _ in 0..3 {
            transport.replies.push_back(Ok(bytes.clone()));
        }
        assert!(read_edid(&mut transport)
            .unwrap_err()
            .contains("invalid monitor Plug and Play ID"));
        assert_eq!(transport.writes.len(), 3);
    }

    #[test]
    fn control_retries_transport_and_checksum_errors_and_decodes_big_endian_values() {
        let valid = reply(&[0x02, 0, 0x10, 0, 0x12, 0x34, 0x01, 0x23]);
        let mut corrupt = valid.clone();
        corrupt[3] ^= 1;
        let mut transport = MockTransport::default();
        transport.replies.extend([
            Err(io::Error::from(io::ErrorKind::Interrupted)),
            Ok(corrupt),
            Ok(valid),
        ]);
        assert_eq!(
            read_control(&mut transport, 0x10).unwrap(),
            Some(Reading {
                current: 0x123,
                maximum: 0x1234
            })
        );
        assert_eq!(transport.writes.len(), 3);
        assert!(transport
            .writes
            .iter()
            .all(|(_, bytes)| bytes[2..4] == [0x01, 0x10]));
    }

    #[test]
    fn unsupported_controls_are_not_retried_and_wrong_control_replies_are_rejected() {
        let mut transport = MockTransport::default();
        transport
            .replies
            .push_back(Ok(reply(&[0x02, 1, 0x10, 0, 0, 0, 0, 0])));
        assert_eq!(read_control(&mut transport, 0x10).unwrap(), None);
        assert_eq!(transport.writes.len(), 1);
        for _ in 0..3 {
            transport
                .replies
                .push_back(Ok(reply(&[0x02, 0, 0x12, 0, 0, 100, 0, 50])));
        }
        assert!(read_control(&mut transport, 0x10)
            .unwrap_err()
            .contains("UnexpectedControl"));
        assert_eq!(transport.writes.len(), 4);
    }

    #[test]
    fn capabilities_validate_offsets_retry_fragments_and_require_termination() {
        let mut transport = MockTransport::default();
        transport.replies.extend([
            Ok(caps_reply(0, b"(vcp(")),
            Ok(caps_reply(0, b"")), // A stale terminal reply cannot end the exchange.
            Ok(caps_reply(5, b"10))")),
            Ok(caps_reply(9, b"")),
        ]);
        assert_eq!(read_capabilities(&mut transport).unwrap(), "(vcp(10))");
        let requests = transport
            .writes
            .iter()
            .map(|(_, bytes)| &bytes[2..5])
            .collect::<Vec<_>>();
        assert_eq!(
            requests,
            [
                &[0xf3, 0, 0][..],
                &[0xf3, 0, 5],
                &[0xf3, 0, 5],
                &[0xf3, 0, 9]
            ]
        );
        assert!(transport.replies.is_empty());
    }

    #[test]
    fn capabilities_never_wrap_the_sixteen_bit_offset() {
        let mut transport = MockTransport::default();
        let mut offset = 0usize;
        while offset < usize::from(u16::MAX) {
            let length = 61.min(usize::from(u16::MAX) - offset);
            transport
                .replies
                .push_back(Ok(caps_reply(offset as u16, &vec![b' '; length])));
            offset += length;
        }
        transport
            .replies
            .push_back(Ok(caps_reply(u16::MAX, b"overflow")));
        assert!(read_capabilities(&mut transport)
            .unwrap_err()
            .contains("65535"));
        assert_eq!(&transport.writes.last().unwrap().1[2..5], &[0xf3, 255, 255]);
        assert!(transport.replies.is_empty());
    }

    #[test]
    fn only_numeric_linux_i2c_device_names_are_accepted() {
        assert_eq!(device_number("i2c-12"), Some(12));
        for name in [
            "i2c-",
            "i2c-+1",
            "i2c--1",
            "i2c-1/../mem",
            "mem",
            "i2c-99999999999999",
        ] {
            assert_eq!(device_number(name), None);
        }
    }

    #[test]
    fn model_fallback_requires_a_complete_model_field() {
        assert_eq!(
            capability_model("(prot(monitor)model(Display & More)vcp(10))").as_deref(),
            Some("Display & More")
        );
        for raw in [
            "(vcp(10))",
            "(model())",
            "(model(unclosed",
            "(othermodel(Wrong))",
        ] {
            assert_eq!(capability_model(raw), None, "{raw}");
        }
    }
}
