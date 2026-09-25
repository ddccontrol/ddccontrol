//! Database source decoding shared by producer and runtime.

use encoding_rs::{Encoding, UTF_8};
use std::{borrow::Cow, fs, path::Path};

pub fn read_xml_file(path: &Path) -> std::io::Result<String> {
    let bytes = fs::read(path)?;
    Ok(normalize_xml_document(
        decode_xml_bytes(&bytes).into_owned(),
    ))
}

pub fn decode_xml_bytes(bytes: &[u8]) -> Cow<'_, str> {
    let encoding = xml_declared_encoding(bytes).unwrap_or(UTF_8);
    let (decoded, _, _) = encoding.decode(bytes);
    decoded
}

pub fn normalize_xml_document(xml: String) -> String {
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
    Encoding::for_label(xml_declared_label(bytes)?)
}

fn xml_declared_label(bytes: &[u8]) -> Option<&[u8]> {
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
    Some(&after_equals[1..label_end])
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

/// A producer must not silently replace malformed source bytes or guess unknown
/// encoding labels. The reader retains its historical tolerant decode entry point.
pub fn decode_source(bytes: &[u8]) -> Result<String, String> {
    let encoding = if let Some((encoding, _)) = Encoding::for_bom(bytes) {
        encoding
    } else if let Some(label) = xml_declared_label(bytes) {
        Encoding::for_label(label).ok_or_else(|| {
            format!(
                "unsupported source XML encoding: {}",
                String::from_utf8_lossy(label)
            )
        })?
    } else {
        UTF_8
    };
    let (decoded, _, errors) = encoding.decode(bytes);
    if errors {
        return Err("malformed source XML encoding".into());
    }
    Ok(normalize_xml_document(decoded.into_owned()))
}
