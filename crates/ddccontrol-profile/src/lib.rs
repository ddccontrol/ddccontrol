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
    let decoded = decode_xml_bytes(input);
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
    push_escaped_attribute(&mut output, &profile.name);
    output.push_str("\" pnpid=\"");
    push_escaped_attribute(&mut output, &profile.pnp_id);
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

fn push_escaped_attribute(output: &mut String, input: &str) {
    for character in input.chars() {
        match character {
            '&' => output.push_str("&amp;"),
            '<' => output.push_str("&lt;"),
            '>' => output.push_str("&gt;"),
            '\"' => output.push_str("&quot;"),
            '\'' => output.push_str("&apos;"),
            _ => output.push(character),
        }
    }
}

fn decode_xml_bytes(bytes: &[u8]) -> Cow<'_, str> {
    let encoding = xml_declared_encoding(bytes).unwrap_or(UTF_8);
    let (decoded, _, _) = encoding.decode(bytes);
    decoded
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

fn xml_declared_encoding(bytes: &[u8]) -> Option<&'static Encoding> {
    let prefix = &bytes[..bytes.len().min(256)];
    let declaration_start = prefix.windows(5).position(|window| window == b"<?xml")?;
    let declaration = &prefix[declaration_start..];
    let declaration_end = declaration
        .windows(2)
        .position(|window| window == b"?>")
        .unwrap_or(declaration.len());
    let declaration = &declaration[..declaration_end];
    let encoding_index = declaration
        .windows("encoding".len())
        .position(|window| window == b"encoding")?;
    let after_encoding = trim_ascii_start(&declaration[encoding_index + "encoding".len()..]);
    let after_equals = trim_ascii_start(after_encoding.strip_prefix(b"=")?);
    let quote = after_equals.first().copied()?;
    if quote != b'\'' && quote != b'\"' {
        return None;
    }
    let label_end = after_equals[1..]
        .iter()
        .position(|byte| *byte == quote)
        .map(|index| index + 1)?;
    Encoding::for_label(&after_equals[1..label_end])
}

fn trim_ascii_start(input: &[u8]) -> &[u8] {
    let start = input
        .iter()
        .position(|byte| !byte.is_ascii_whitespace())
        .unwrap_or(input.len());
    &input[start..]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_version_one_profile_and_strtol_number_syntax() {
        let profile = parse(
            r#"<profile name="Office" pnpid="DEL1234" version="1">
                <control address="0x10" value="75"/>
                <control address="014" value="0xffff"/>
                <ignored/>
            </profile>"#,
        )
        .unwrap();

        assert_eq!(profile.name, "Office");
        assert_eq!(profile.pnp_id, "DEL1234");
        assert_eq!(
            profile.controls,
            vec![
                Control {
                    address: 0x10,
                    value: 75,
                },
                Control {
                    address: 0o14,
                    value: 0xffff,
                },
            ]
        );
    }

    #[test]
    fn serialize_round_trip_escapes_attributes() {
        let profile = Profile {
            name: "Work & \"Play\" <HDR>".to_string(),
            pnp_id: "DEL'1234".to_string(),
            controls: vec![Control {
                address: 0x10,
                value: 0x1234,
            }],
        };

        let xml = serialize(&profile).unwrap();
        assert!(xml.contains("Work &amp; &quot;Play&quot; &lt;HDR&gt;"));
        assert!(xml.contains("DEL&apos;1234"));
        assert_eq!(parse(&xml).unwrap(), profile);
    }

    #[test]
    fn parses_declared_legacy_encoding() {
        let profile = parse_bytes(
            b"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><profile name=\"Contr\xf4le\" pnpid=\"DEL1234\" version=\"1\"/>",
        )
        .unwrap();

        assert_eq!(profile.name, "Contrôle");
    }

    #[test]
    fn accepts_a_comment_before_the_xml_declaration() {
        let profile = parse_bytes(
            b"<!-- generated profile -->\n<?xml version=\"1.0\"?><profile name=\"Office\" pnpid=\"DEL1234\" version=\"1\"/>",
        )
        .unwrap();

        assert_eq!(profile.name, "Office");
    }

    #[test]
    fn rejects_malformed_or_unsupported_profiles() {
        assert!(parse("<monitor/>").is_err());
        assert!(parse(r#"<profile name="x" pnpid="p" version="2"/>"#).is_err());
        assert!(parse(r#"<profile name="x" version="1"/>"#).is_err());
        assert!(parse(
            r#"<profile name="x" pnpid="p" version="1"><control address="256" value="1"/></profile>"#
        )
        .is_err());
        assert!(parse(
            r#"<profile name="x" pnpid="p" version="1"><control address="1" value="65536"/></profile>"#
        )
        .is_err());
    }

    #[test]
    fn rejects_more_controls_than_the_c_abi_can_store() {
        let controls = "<control address=\"1\" value=\"2\"/>".repeat(MAX_CONTROLS + 1);
        let xml = format!("<profile name=\"x\" pnpid=\"p\" version=\"1\">{controls}</profile>");

        assert!(parse(&xml).is_err());
    }
}
