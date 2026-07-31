// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use std::fmt;

pub const EDID_BLOCK_LEN: usize = 128;
pub const EDID_MIN_PARSE_LEN: usize = 0x17;

const EDID_HEADER: [u8; 8] = [0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00];
const DESCRIPTOR_START: usize = 0x36;
const DESCRIPTOR_LEN: usize = 18;
const DESCRIPTOR_TEXT_LEN: usize = 13;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Edid {
    pnp_id: String,
    digital_input: bool,
    raw: Vec<u8>,
    info: EdidInfo,
}

impl Edid {
    pub fn pnp_id(&self) -> &str {
        &self.pnp_id
    }

    pub fn is_digital_input(&self) -> bool {
        self.digital_input
    }

    pub fn raw(&self) -> &[u8] {
        &self.raw
    }

    pub fn info(&self) -> &EdidInfo {
        &self.info
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdidInfo {
    pub serial_number: u32,
    pub manufacture_week: u8,
    pub manufacture_year: u16,
    pub version: u8,
    pub revision: u8,
    pub max_width_cm: u8,
    pub max_height_cm: u8,
    pub monitor_name: String,
    pub serial_ascii: String,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ParseError {
    TooShort { actual: usize },
    InvalidHeader,
}

impl fmt::Display for ParseError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::TooShort { actual } => write!(
                formatter,
                "EDID buffer is {actual} bytes; at least {EDID_MIN_PARSE_LEN} are required"
            ),
            Self::InvalidHeader => formatter.write_str("invalid EDID header"),
        }
    }
}

impl std::error::Error for ParseError {}

/// Parse the fields used by ddccontrol from an EDID base block.
///
/// The historical C API accepts a partial block once all fields through the
/// maximum-image-size bytes are present. Descriptor text is therefore parsed
/// only when a complete 128-byte base block is available. The checksum is not
/// validated in order to retain compatibility with monitors accepted by the C
/// implementation.
pub fn parse(input: &[u8]) -> Result<Edid, ParseError> {
    if input.len() < EDID_MIN_PARSE_LEN {
        return Err(ParseError::TooShort {
            actual: input.len(),
        });
    }
    if input[..EDID_HEADER.len()] != EDID_HEADER {
        return Err(ParseError::InvalidHeader);
    }

    let manufacturer = [
        ((input[8] >> 2) & 0x1f) + b'A' - 1,
        ((input[8] & 0x03) << 3) + (input[9] >> 5) + b'A' - 1,
        (input[9] & 0x1f) + b'A' - 1,
    ];
    let product_code = u16::from_le_bytes([input[10], input[11]]);
    let mut pnp_id = String::with_capacity(7);
    pnp_id.extend(manufacturer.into_iter().map(char::from));
    use std::fmt::Write;
    write!(&mut pnp_id, "{product_code:04X}").expect("writing to a String cannot fail");

    let mut info = EdidInfo {
        serial_number: u32::from_le_bytes([input[0x0c], input[0x0d], input[0x0e], input[0x0f]]),
        manufacture_week: input[0x10],
        manufacture_year: u16::from(input[0x11]) + 1990,
        version: input[0x12],
        revision: input[0x13],
        max_width_cm: input[0x15],
        max_height_cm: input[0x16],
        monitor_name: String::new(),
        serial_ascii: String::new(),
    };

    if input.len() >= EDID_BLOCK_LEN {
        parse_descriptors(input, &mut info);
    }

    Ok(Edid {
        pnp_id,
        digital_input: input[0x14] & 0x80 != 0,
        raw: input[..input.len().min(EDID_BLOCK_LEN)].to_vec(),
        info,
    })
}

fn parse_descriptors(input: &[u8], info: &mut EdidInfo) {
    for descriptor in 0..4 {
        let start = DESCRIPTOR_START + descriptor * DESCRIPTOR_LEN;
        let descriptor = &input[start..start + DESCRIPTOR_LEN];

        if descriptor[0] != 0 || descriptor[1] != 0 || descriptor[2] != 0 || descriptor[4] != 0 {
            continue;
        }

        match descriptor[3] {
            0xfc => info.monitor_name = parse_descriptor_text(&descriptor[5..]),
            0xff => info.serial_ascii = parse_descriptor_text(&descriptor[5..]),
            _ => {}
        }
    }
}

fn parse_descriptor_text(input: &[u8]) -> String {
    let mut text = Vec::with_capacity(DESCRIPTOR_TEXT_LEN);

    for &byte in input.iter().take(DESCRIPTOR_TEXT_LEN) {
        if byte == b'\n' || byte == b'\r' {
            break;
        }
        text.push(if (b' '..=b'~').contains(&byte) {
            byte
        } else {
            b' '
        });
    }

    while text.last() == Some(&b' ') {
        text.pop();
    }

    String::from_utf8(text).expect("descriptor normalization only emits ASCII")
}
