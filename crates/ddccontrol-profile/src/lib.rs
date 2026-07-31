// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use encoding_rs::{Encoding, UTF_8};
use roxmltree::Document;
use std::borrow::Cow;
use std::fmt;

pub const PROFILE_VERSION: i64 = 1;
pub const MAX_CONTROLS: usize = 256;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Profile {
    pub name: String,
    pub pnp_id: String,
    pub controls: Vec<Control>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Control {
    pub address: u8,
    pub value: u16,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ProfileError(String);

impl ProfileError {
    fn new(message: impl Into<String>) -> Self {
        Self(message.into())
    }
}

impl fmt::Display for ProfileError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

impl std::error::Error for ProfileError {}

pub fn parse_bytes(input: &[u8]) -> Result<Profile, ProfileError> {
    let decoded = decode_xml_bytes(input)?;
    parse(&normalize_xml_document(decoded.into_owned()))
}

pub fn parse(input: &str) -> Result<Profile, ProfileError> {
    let document = Document::parse(input).map_err(|error| ProfileError::new(error.to_string()))?;
    let root = document.root_element();
    if root.tag_name().name() != "profile" {
        return Err(ProfileError::new(format!(
            "profile has root element {}, expected profile",
            root.tag_name().name()
        )));
    }

    let name = required_attribute(root, "name")?.to_string();
    let pnp_id = required_attribute(root, "pnpid")?.to_string();
    let version = parse_integer(required_attribute(root, "version")?)
        .map_err(|_| ProfileError::new("profile version is not a valid integer"))?;
    if version != PROFILE_VERSION {
        return Err(ProfileError::new(format!(
            "profile version {version} is not supported"
        )));
    }

    let mut controls = Vec::new();
    for node in root.children().filter(|node| node.is_element()) {
        if node.tag_name().name() != "control" {
            continue;
        }
        if controls.len() == MAX_CONTROLS {
            return Err(ProfileError::new(format!(
                "profile contains more than {MAX_CONTROLS} controls"
            )));
        }

        let address =
            parse_bounded_integer(required_attribute(node, "address")?, u64::from(u8::MAX))
                .map_err(|message| {
                    ProfileError::new(format!("invalid control address: {message}"))
                })?;
        let value = parse_bounded_integer(required_attribute(node, "value")?, u64::from(u16::MAX))
            .map_err(|message| ProfileError::new(format!("invalid control value: {message}")))?;
        controls.push(Control {
            address: address as u8,
            value: value as u16,
        });
    }

    Ok(Profile {
        name,
        pnp_id,
        controls,
    })
}

pub fn serialize(profile: &Profile) -> Result<String, ProfileError> {
    if profile.controls.len() > MAX_CONTROLS {
        return Err(ProfileError::new(format!(
            "profile contains more than {MAX_CONTROLS} controls"
        )));
    }

    let mut output = String::from("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<profile name=\"");
    push_escaped_attribute(&mut output, &profile.name)?;
    output.push_str("\" pnpid=\"");
    push_escaped_attribute(&mut output, &profile.pnp_id)?;
    output.push_str("\" version=\"1\">\n");
    for control in &profile.controls {
        output.push_str(&format!(
            "  <control address=\"{:#x}\" value=\"{:#x}\"/>\n",
            control.address, control.value
        ));
    }
    output.push_str("</profile>\n");
    Ok(output)
}

fn required_attribute<'a, 'input>(
    node: roxmltree::Node<'a, 'input>,
    name: &str,
) -> Result<&'a str, ProfileError> {
    node.attribute(name)
        .ok_or_else(|| ProfileError::new(format!("missing {name} attribute")))
}

fn parse_bounded_integer(input: &str, maximum: u64) -> Result<u64, String> {
    let value = parse_integer(input).map_err(|_| format!("{input:?} is not an integer"))?;
    if value < 0 || value as u64 > maximum {
        return Err(format!("{value} is outside 0..={maximum}"));
    }
    Ok(value as u64)
}

fn parse_integer(input: &str) -> Result<i64, std::num::ParseIntError> {
    let input = input.trim_start_matches(|character: char| character.is_ascii_whitespace());
    let (negative, rest) = if let Some(rest) = input.strip_prefix('-') {
        (true, rest)
    } else if let Some(rest) = input.strip_prefix('+') {
        (false, rest)
    } else {
        (false, input)
    };
    let (radix, digits) =
        if let Some(rest) = rest.strip_prefix("0x").or_else(|| rest.strip_prefix("0X")) {
            (16, rest)
        } else if rest.len() > 1 && rest.starts_with('0') {
            (8, &rest[1..])
        } else {
            (10, rest)
        };
    let value = i64::from_str_radix(digits, radix)?;
    Ok(if negative { -value } else { value })
}

fn push_escaped_attribute(output: &mut String, input: &str) -> Result<(), ProfileError> {
    for character in input.chars() {
        match character {
            '&' => output.push_str("&amp;"),
            '<' => output.push_str("&lt;"),
            '>' => output.push_str("&gt;"),
            '\"' => output.push_str("&quot;"),
            '\'' => output.push_str("&apos;"),
            '\t' => output.push_str("&#x9;"),
            '\n' => output.push_str("&#xA;"),
            '\r' => output.push_str("&#xD;"),
            character if is_xml_character(character) => output.push(character),
            character => {
                return Err(ProfileError::new(format!(
                    "profile contains invalid XML character U+{:04X}",
                    u32::from(character)
                )))
            }
        }
    }
    Ok(())
}

fn is_xml_character(character: char) -> bool {
    matches!(
        u32::from(character),
        0x20..=0xD7FF | 0xE000..=0xFFFD | 0x10000..=0x10FFFF
    )
}

fn decode_xml_bytes(bytes: &[u8]) -> Result<Cow<'_, str>, ProfileError> {
    let encoding = xml_declared_encoding(bytes)?.unwrap_or(UTF_8);
    let (decoded, actual_encoding, had_errors) = encoding.decode(bytes);
    if had_errors {
        return Err(ProfileError::new(format!(
            "profile contains bytes that are invalid for {}",
            actual_encoding.name()
        )));
    }
    Ok(decoded)
}

fn normalize_xml_document(xml: String) -> String {
    let mut cursor = 0;
    loop {
        cursor += xml[cursor..]
            .find(|character: char| !character.is_whitespace())
            .unwrap_or(xml.len() - cursor);
        if !xml[cursor..].starts_with("<!--") {
            break;
        }
        let Some(comment_end) = xml[cursor + 4..].find("-->") else {
            return xml;
        };
        cursor += 4 + comment_end + 3;
    }

    if cursor > 0 && xml[cursor..].starts_with("<?xml") {
        xml[cursor..].to_string()
    } else {
        xml
    }
}

fn xml_declared_encoding(bytes: &[u8]) -> Result<Option<&'static Encoding>, ProfileError> {
    let Some(declaration) = xml_declaration(bytes) else {
        return Ok(None);
    };
    let Some(encoding_index) = xml_attribute(declaration, b"encoding") else {
        return Ok(None);
    };
    let after_encoding = trim_ascii_start(&declaration[encoding_index + "encoding".len()..]);
    let after_equals =
        trim_ascii_start(after_encoding.strip_prefix(b"=").ok_or_else(|| {
            ProfileError::new("XML encoding declaration is missing an equals sign")
        })?);
    let quote = after_equals
        .first()
        .copied()
        .filter(|quote| *quote == b'\'' || *quote == b'\"')
        .ok_or_else(|| ProfileError::new("XML encoding declaration is not quoted"))?;
    let label_end = after_equals[1..]
        .iter()
        .position(|byte| *byte == quote)
        .map(|index| index + 1)
        .ok_or_else(|| ProfileError::new("XML encoding declaration has no closing quote"))?;
    let label = &after_equals[1..label_end];
    Encoding::for_label(label).map(Some).ok_or_else(|| {
        ProfileError::new(format!(
            "XML declares unsupported encoding {:?}",
            String::from_utf8_lossy(label)
        ))
    })
}

fn xml_declaration(bytes: &[u8]) -> Option<&[u8]> {
    let mut cursor = if bytes.starts_with(&[0xEF, 0xBB, 0xBF]) {
        3
    } else {
        0
    };
    loop {
        cursor += bytes[cursor..]
            .iter()
            .position(|byte| !byte.is_ascii_whitespace())
            .unwrap_or(bytes.len() - cursor);
        if !bytes[cursor..].starts_with(b"<!--") {
            break;
        }
        let comment_end = bytes[cursor + 4..]
            .windows(3)
            .position(|window| window == b"-->")?;
        cursor += 4 + comment_end + 3;
    }

    let declaration = bytes[cursor..].strip_prefix(b"<?xml")?;
    let declaration_end = declaration
        .windows(2)
        .position(|window| window == b"?>")
        .unwrap_or(declaration.len());
    Some(&declaration[..declaration_end])
}

fn xml_attribute(input: &[u8], name: &[u8]) -> Option<usize> {
    input
        .windows(name.len())
        .enumerate()
        .find_map(|(index, window)| {
            if window != name {
                return None;
            }
            let preceded_by_whitespace = index > 0 && input[index - 1].is_ascii_whitespace();
            let following = input.get(index + name.len()).copied();
            (preceded_by_whitespace
                && following.is_some_and(|byte| byte == b'=' || byte.is_ascii_whitespace()))
            .then_some(index)
        })
}

fn trim_ascii_start(input: &[u8]) -> &[u8] {
    let start = input
        .iter()
        .position(|byte| !byte.is_ascii_whitespace())
        .unwrap_or(input.len());
    &input[start..]
}
