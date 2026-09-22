// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use encoding_rs::{Encoding, UTF_16BE, UTF_16LE, UTF_8};
use roxmltree::Document;
use std::fmt;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Monitor {
    pub filename: String,
    pub supported: u8,
    pub name: String,
    pub digital: u8,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MonitorListError(String);

impl MonitorListError {
    fn new(message: impl Into<String>) -> Self {
        Self(message.into())
    }
}

impl fmt::Display for MonitorListError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

impl std::error::Error for MonitorListError {}

/// Decode a cached XML file, including BOMs and declared legacy encodings.
pub fn parse_bytes(input: &[u8], version: &str) -> Result<Vec<Monitor>, MonitorListError> {
    let encoding = if let Some((encoding, _)) = Encoding::for_bom(input) {
        encoding
    } else if input.starts_with(b"\0<\0?") {
        UTF_16BE
    } else if input.starts_with(b"<\0?\0") {
        UTF_16LE
    } else {
        declared_encoding(input)?.unwrap_or(UTF_8)
    };
    let (decoded, _, had_errors) = encoding.decode(input);
    if had_errors {
        return Err(MonitorListError::new(format!(
            "invalid {} monitor-list XML",
            encoding.name()
        )));
    }
    // A BOM determines decoding, but must not hide an unsupported declaration.
    declared_encoding(decoded.as_bytes())?;
    parse(&decoded, version)
}

/// Parse only direct monitor children, preserving their order and duplicates.
/// A cache is valid only for the exact ddccontrol version that wrote it.
pub fn parse(input: &str, version: &str) -> Result<Vec<Monitor>, MonitorListError> {
    let document =
        Document::parse(input).map_err(|error| MonitorListError::new(error.to_string()))?;
    let root = document.root_element();
    if root.tag_name().name() != "monitorlist" {
        return Err(MonitorListError::new("expected monitorlist root element"));
    }
    let saved_version = required_attribute(root, "ddccontrolversion")?;
    if saved_version != version {
        return Err(MonitorListError::new(format!(
            "monitor-list version {saved_version:?} does not match {version:?}"
        )));
    }

    root.children()
        .filter(|node| node.is_element() && node.tag_name().name() == "monitor")
        .map(|node| {
            Ok(Monitor {
                filename: required_attribute(node, "filename")?.to_string(),
                supported: parse_byte(required_attribute(node, "supported")?)?,
                name: required_attribute(node, "name")?.to_string(),
                digital: parse_byte(required_attribute(node, "digital")?)?,
            })
        })
        .collect()
}

/// Serialize the existing cache schema as UTF-8, with decimal flag values.
pub fn serialize(monitors: &[Monitor], version: &str) -> Result<String, MonitorListError> {
    let mut output = String::from(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<monitorlist ddccontrolversion=\"",
    );
    push_attribute(&mut output, version)?;
    output.push_str("\">\n");
    for monitor in monitors {
        output.push_str("  <monitor filename=\"");
        push_attribute(&mut output, &monitor.filename)?;
        output.push_str(&format!("\" supported=\"{}\" name=\"", monitor.supported));
        push_attribute(&mut output, &monitor.name)?;
        output.push_str(&format!("\" digital=\"{}\"/>\n", monitor.digital));
    }
    output.push_str("</monitorlist>\n");
    Ok(output)
}

fn required_attribute<'a>(
    node: roxmltree::Node<'a, '_>,
    name: &str,
) -> Result<&'a str, MonitorListError> {
    node.attribute(name)
        .ok_or_else(|| MonitorListError::new(format!("missing {name} attribute")))
}

fn parse_byte(input: &str) -> Result<u8, MonitorListError> {
    // Match strtol's base-zero syntax, but reject empty and out-of-range values
    // instead of silently truncating them into the C unsigned-char fields.
    let value = input.trim_start_matches(|character: char| character.is_ascii_whitespace());
    let value = value.strip_prefix('+').unwrap_or(value);
    let (radix, digits) = if let Some(digits) = value
        .strip_prefix("0x")
        .or_else(|| value.strip_prefix("0X"))
    {
        (16, digits)
    } else if value.len() > 1 && value.starts_with('0') {
        (8, &value[1..])
    } else {
        (10, value)
    };
    // from_str_radix accepts a leading '+', which must not be accepted twice.
    if digits.is_empty() || !digits.chars().all(|character| character.is_digit(radix)) {
        return Err(MonitorListError::new(format!(
            "invalid monitor flag {input:?}"
        )));
    }
    u8::from_str_radix(digits, radix)
        .map_err(|_| MonitorListError::new(format!("monitor flag {input:?} is outside 0..=255")))
}

fn push_attribute(output: &mut String, input: &str) -> Result<(), MonitorListError> {
    for character in input.chars() {
        match character {
            '&' => output.push_str("&amp;"),
            '<' => output.push_str("&lt;"),
            '>' => output.push_str("&gt;"),
            '"' => output.push_str("&quot;"),
            '\'' => output.push_str("&apos;"),
            '\t' => output.push_str("&#x9;"),
            '\n' => output.push_str("&#xA;"),
            '\r' => output.push_str("&#xD;"),
            '\u{20}'..='\u{D7FF}' | '\u{E000}'..='\u{FFFD}' | '\u{10000}'..='\u{10FFFF}' => {
                output.push(character)
            }
            _ => {
                return Err(MonitorListError::new(format!(
                    "invalid XML character U+{:04X}",
                    u32::from(character)
                )))
            }
        }
    }
    Ok(())
}

fn declared_encoding(input: &[u8]) -> Result<Option<&'static Encoding>, MonitorListError> {
    let Some(declaration) = input
        .strip_prefix(b"<?xml")
        .filter(|rest| rest.first().is_some_and(u8::is_ascii_whitespace))
    else {
        return Ok(None);
    };
    let end = declaration
        .windows(2)
        .position(|bytes| bytes == b"?>")
        .ok_or_else(|| MonitorListError::new("unterminated XML declaration"))?;
    let declaration = &declaration[..end];
    let Some(index) = declaration
        .windows(8)
        .enumerate()
        .find_map(|(index, bytes)| {
            (bytes == b"encoding" && index > 0 && declaration[index - 1].is_ascii_whitespace())
                .then_some(index)
        })
    else {
        return Ok(None);
    };
    let invalid = || MonitorListError::new("invalid XML encoding declaration");
    let rest = trim_ascii_start(&declaration[index + 8..]);
    let rest = trim_ascii_start(rest.strip_prefix(b"=").ok_or_else(invalid)?);
    let quote = *rest
        .first()
        .filter(|quote| **quote == b'\'' || **quote == b'"')
        .ok_or_else(invalid)?;
    let end = rest[1..]
        .iter()
        .position(|byte| *byte == quote)
        .ok_or_else(invalid)?;
    let label = &rest[1..1 + end];
    Encoding::for_label(label)
        .map(Some)
        .ok_or_else(|| MonitorListError::new("unsupported XML encoding"))
}

fn trim_ascii_start(input: &[u8]) -> &[u8] {
    let start = input
        .iter()
        .position(|byte| !byte.is_ascii_whitespace())
        .unwrap_or(input.len());
    &input[start..]
}
