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
    // Cache flags are unsigned, including their textual representation.
    let invalid = || MonitorListError::new(format!("invalid monitor flag {input:?}"));
    if input
        .trim_start_matches(|character: char| character.is_ascii_whitespace())
        .starts_with('-')
    {
        return Err(invalid());
    }
    let value = ddccontrol_xml::parse_integer(input).map_err(|_| invalid())?;
    u8::try_from(value)
        .map_err(|_| MonitorListError::new(format!("monitor flag {input:?} is outside 0..=255")))
}

fn push_attribute(output: &mut String, input: &str) -> Result<(), MonitorListError> {
    ddccontrol_xml::push_attribute(output, input).map_err(|character| {
        MonitorListError::new(format!(
            "invalid XML character U+{:04X}",
            u32::from(character)
        ))
    })
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
