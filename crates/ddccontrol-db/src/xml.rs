// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Database XML decoding and diagnostics shared by options and monitor definitions.

use encoding_rs::{Encoding, UTF_8};
use roxmltree::Node;
use std::{borrow::Cow, fs, path::Path};

pub(crate) fn read_xml_file(path: &Path) -> std::io::Result<String> {
    let bytes = fs::read(path)?;
    Ok(normalize_xml_document(
        decode_xml_bytes(&bytes).into_owned(),
    ))
}

pub(crate) fn decode_xml_bytes(bytes: &[u8]) -> Cow<'_, str> {
    let encoding = xml_declared_encoding(bytes).unwrap_or(UTF_8);
    let (decoded, _, _) = encoding.decode(bytes);
    decoded
}

pub(crate) fn normalize_xml_document(xml: String) -> String {
    let mut cursor = 0;
    loop {
        cursor += xml[cursor..]
            .find(|ch: char| !ch.is_whitespace())
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

fn xml_declared_encoding(bytes: &[u8]) -> Option<&'static Encoding> {
    let declaration_start = xml_declaration_start(bytes)?;
    let prefix = &bytes[declaration_start..];
    let prefix = &prefix[..prefix.len().min(256)];
    let declaration_end = prefix
        .windows(2)
        .position(|window| window == b"?>")
        .unwrap_or(prefix.len());
    let declaration = &prefix[..declaration_end];
    let encoding_index = declaration
        .windows("encoding".len())
        .position(|window| window == b"encoding")?;
    let after_encoding = &declaration[encoding_index + "encoding".len()..];
    let after_encoding = trim_ascii_bytes_start(after_encoding);
    let after_equals = trim_ascii_bytes_start(after_encoding.strip_prefix(b"=")?);
    let quote = after_equals.first().copied()?;
    if quote != b'\'' && quote != b'"' {
        return None;
    }
    let label_end = after_equals[1..]
        .iter()
        .position(|byte| *byte == quote)
        .map(|index| index + 1)?;
    Encoding::for_label(&after_equals[1..label_end])
}

fn xml_declaration_start(bytes: &[u8]) -> Option<usize> {
    let mut cursor = 0;
    loop {
        cursor += bytes[cursor..]
            .iter()
            .position(|byte| !byte.is_ascii_whitespace())?;
        if bytes[cursor..].starts_with(b"<?xml") {
            return Some(cursor);
        }
        if !bytes[cursor..].starts_with(b"<!--") {
            return None;
        }
        let comment_end = bytes[cursor + 4..]
            .windows(3)
            .position(|window| window == b"-->")?;
        cursor += 4 + comment_end + 3;
    }
}

fn trim_ascii_bytes_start(input: &[u8]) -> &[u8] {
    let start = input
        .iter()
        .position(|byte| !byte.is_ascii_whitespace())
        .unwrap_or(input.len());
    &input[start..]
}

pub(crate) fn required_attr<'a, 'd>(node: Node<'a, 'd>, name: &str) -> Result<&'a str, String> {
    node.attribute(name)
        .ok_or_else(|| node_error(node, &format!("Can't find {name} property.")))
}

pub(crate) fn element_children<'a, 'd>(node: Node<'a, 'd>) -> impl Iterator<Item = Node<'a, 'd>> {
    node.children().filter(|child| child.is_element())
}

pub(crate) fn node_error(node: Node<'_, '_>, message: &str) -> String {
    format!("Error: {message} @line {}", line(node))
}

pub(crate) fn line(node: Node<'_, '_>) -> u32 {
    node.document().text_pos_at(node.range().start).row
}
