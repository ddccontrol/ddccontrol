// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

//! Small format helpers shared by the database, profiles, cache and scanner.
//! XML document parsing remains the responsibility of `roxmltree`.

/// Parse the database's signed decimal, hexadecimal and octal notation.
pub fn parse_integer(input: &str) -> Result<i64, &'static str> {
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
    // from_str_radix accepts a sign itself; do not allow a second one after
    // stripping our sign or radix prefix (for example, ++1 or 0x-1).
    if digits.is_empty() || !digits.chars().all(|character| character.is_digit(radix)) {
        return Err("invalid integer syntax");
    }
    if negative {
        i64::from_str_radix(&format!("-{digits}"), radix)
    } else {
        i64::from_str_radix(digits, radix)
    }
    .map_err(|_| "integer outside the signed 64-bit range")
}

/// Whether a character can appear in an XML 1.0 document.
pub fn is_xml_character(character: char) -> bool {
    matches!(character, '\t' | '\n' | '\r' | '\u{20}'..='\u{d7ff}' |
        '\u{e000}'..='\u{fffd}' | '\u{10000}'..='\u{10ffff}')
}

/// Append an attribute value, preserving whitespace through character references.
/// Returns the first invalid XML character; output may already contain a prefix.
pub fn push_attribute(output: &mut String, input: &str) -> Result<(), char> {
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
            character if is_xml_character(character) => output.push(character),
            character => return Err(character),
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn database_integer_notation() {
        for (text, expected) in [
            ("010", 8),
            ("+0x10", 16),
            (" 65535", 65535),
            ("-1", -1),
            ("-9223372036854775808", i64::MIN),
        ] {
            assert_eq!(parse_integer(text).unwrap(), expected);
        }
        for invalid in [
            "09",
            "1 ",
            "bad",
            "",
            "18446744073709551616",
            "++1",
            "--1",
            "0x+1",
            "0x-1",
            "--9223372036854775808",
        ] {
            assert!(parse_integer(invalid).is_err());
        }
    }
}
