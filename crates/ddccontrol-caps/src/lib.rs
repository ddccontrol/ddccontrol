use std::collections::BTreeMap;
use std::fmt;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum MonitorType {
    #[default]
    Unknown,
    Lcd,
    Crt,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VcpEntry {
    values: Option<Vec<u16>>,
}

impl VcpEntry {
    pub fn all_values() -> Self {
        Self { values: None }
    }

    pub fn with_values(values: Vec<u16>) -> Self {
        Self {
            values: Some(values),
        }
    }

    pub fn values(&self) -> Option<&[u16]> {
        self.values.as_deref()
    }
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Caps {
    monitor_type: MonitorType,
    vcp: BTreeMap<u8, VcpEntry>,
}

impl Caps {
    pub fn parse(input: &str) -> Result<Self, ParseError> {
        let mut caps = Self::default();
        caps.apply(input, Edit::Add)?;
        Ok(caps)
    }

    pub fn apply_add(&mut self, input: &str) -> Result<usize, ParseError> {
        self.apply(input, Edit::Add)
    }

    pub fn apply_remove(&mut self, input: &str) -> Result<usize, ParseError> {
        self.apply(input, Edit::Remove)
    }

    pub fn monitor_type(&self) -> MonitorType {
        self.monitor_type
    }

    pub fn is_supported(&self, code: u8) -> bool {
        self.vcp.contains_key(&code)
    }

    pub fn vcp(&self, code: u8) -> Option<&VcpEntry> {
        self.vcp.get(&code)
    }

    pub fn vcp_codes(&self) -> impl Iterator<Item = u8> + '_ {
        self.vcp.keys().copied()
    }

    pub fn vcp_entries(&self) -> impl Iterator<Item = (u8, &VcpEntry)> + '_ {
        self.vcp.iter().map(|(&code, entry)| (code, entry))
    }

    pub fn set_monitor_type(&mut self, monitor_type: MonitorType) {
        self.monitor_type = monitor_type;
    }

    pub fn insert_vcp(&mut self, code: u8, entry: VcpEntry) {
        self.vcp.insert(code, entry);
    }

    fn apply(&mut self, input: &str, edit: Edit) -> Result<usize, ParseError> {
        let parsed = Parser::new(input).parse()?;

        if edit == Edit::Add {
            if let Some(monitor_type) = parsed.monitor_type {
                self.monitor_type = monitor_type;
            }
        }

        for item in &parsed.vcp {
            match edit {
                Edit::Add => {
                    self.vcp.insert(
                        item.code,
                        match &item.values {
                            Some(values) => VcpEntry::with_values(values.clone()),
                            None => VcpEntry::all_values(),
                        },
                    );
                }
                Edit::Remove => match &item.values {
                    Some(remove_values) => {
                        if let Some(entry) = self.vcp.get_mut(&item.code) {
                            if let Some(values) = &mut entry.values {
                                values.retain(|value| !remove_values.contains(value));
                                if values.is_empty() {
                                    self.vcp.remove(&item.code);
                                }
                            } else {
                                self.vcp.remove(&item.code);
                            }
                        }
                    }
                    None => {
                        self.vcp.remove(&item.code);
                    }
                },
            }
        }

        Ok(parsed.vcp.len())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ParseError {
    position: usize,
    kind: ErrorKind,
}

impl ParseError {
    pub fn position(&self) -> usize {
        self.position
    }

    pub fn kind(&self) -> ErrorKind {
        self.kind
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} at byte {}", self.kind, self.position)
    }
}

impl std::error::Error for ParseError {}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ErrorKind {
    InvalidHex,
    UnexpectedCloseParen,
    UnexpectedEnd,
    UnbalancedParens,
    ValueWithoutVcpCode,
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidHex => f.write_str("invalid hexadecimal value"),
            Self::UnexpectedCloseParen => f.write_str("unexpected closing parenthesis"),
            Self::UnexpectedEnd => f.write_str("unexpected end of capability string"),
            Self::UnbalancedParens => f.write_str("unbalanced parentheses"),
            Self::ValueWithoutVcpCode => f.write_str("value list without VCP code"),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Edit {
    Add,
    Remove,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
struct ParsedCaps {
    monitor_type: Option<MonitorType>,
    vcp: Vec<ParsedVcp>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ParsedVcp {
    code: u8,
    values: Option<Vec<u16>>,
}

struct Parser<'a> {
    input: &'a str,
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(input: &'a str) -> Self {
        Self {
            input,
            bytes: input.as_bytes(),
            pos: 0,
        }
    }

    fn parse(mut self) -> Result<ParsedCaps, ParseError> {
        let mut parsed = ParsedCaps::default();
        self.parse_sections(None, &mut parsed)?;
        Ok(parsed)
    }

    fn parse_sections(
        &mut self,
        stop_at: Option<u8>,
        parsed: &mut ParsedCaps,
    ) -> Result<(), ParseError> {
        loop {
            self.skip_spaces();

            match self.peek() {
                None if stop_at.is_some() => return Err(self.err(ErrorKind::UnbalancedParens)),
                None => return Ok(()),
                Some(byte) if Some(byte) == stop_at => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b')') => return Err(self.err(ErrorKind::UnexpectedCloseParen)),
                Some(b'(') => {
                    self.pos += 1;
                    self.parse_sections(Some(b')'), parsed)?;
                }
                Some(byte) if is_name_byte(byte) => {
                    let name = self.take_name();
                    self.skip_spaces();
                    if self.consume(b'(') {
                        match name {
                            "type" => self.parse_type(parsed)?,
                            "vcp" => self.parse_vcp(parsed)?,
                            _ => self.skip_balanced()?,
                        }
                    }
                }
                Some(_) => {
                    self.pos += 1;
                }
            }
        }
    }

    fn parse_type(&mut self, parsed: &mut ParsedCaps) -> Result<(), ParseError> {
        self.skip_spaces();
        let start = self.pos;

        while let Some(byte) = self.peek() {
            if byte == b')' || byte.is_ascii_whitespace() {
                break;
            }
            self.pos += 1;
        }

        let value = self.input[start..self.pos].to_ascii_lowercase();
        match value.as_str() {
            "lcd" => parsed.monitor_type = Some(MonitorType::Lcd),
            "crt" => parsed.monitor_type = Some(MonitorType::Crt),
            _ => {}
        }

        self.skip_balanced_from_current()
    }

    fn parse_vcp(&mut self, parsed: &mut ParsedCaps) -> Result<(), ParseError> {
        loop {
            self.skip_spaces();

            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b'(') => return Err(self.err(ErrorKind::ValueWithoutVcpCode)),
                Some(_) => {
                    let code_position = self.pos;
                    let code = self.take_vcp_code(code_position)?;
                    self.skip_spaces();

                    let values = if self.consume(b'(') {
                        match self.parse_vcp_values()? {
                            values if values.is_empty() => None,
                            values => Some(values),
                        }
                    } else {
                        None
                    };

                    parsed.vcp.push(ParsedVcp { code, values });
                }
            }
        }
    }

    fn parse_vcp_values(&mut self) -> Result<Vec<u16>, ParseError> {
        let mut values = Vec::new();

        loop {
            self.skip_spaces();

            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(values);
                }
                Some(b'(') => {
                    self.pos += 1;
                    self.skip_balanced()?;
                }
                Some(_) => {
                    let value_position = self.pos;
                    let token = self.take_token()?;
                    let value = parse_hex_u16(token)
                        .ok_or_else(|| self.err_at(ErrorKind::InvalidHex, value_position))?;
                    values.push(value);
                }
            }
        }
    }

    fn skip_balanced(&mut self) -> Result<(), ParseError> {
        let mut depth = 1usize;

        while let Some(byte) = self.peek() {
            self.pos += 1;
            match byte {
                b'(' => depth += 1,
                b')' => {
                    depth -= 1;
                    if depth == 0 {
                        return Ok(());
                    }
                }
                _ => {}
            }
        }

        Err(self.err(ErrorKind::UnbalancedParens))
    }

    fn skip_balanced_from_current(&mut self) -> Result<(), ParseError> {
        loop {
            match self.peek() {
                None => return Err(self.err(ErrorKind::UnbalancedParens)),
                Some(b')') => {
                    self.pos += 1;
                    return Ok(());
                }
                Some(b'(') => {
                    self.pos += 1;
                    self.skip_balanced()?;
                }
                Some(_) => {
                    self.pos += 1;
                }
            }
        }
    }

    fn take_vcp_code(&mut self, position: usize) -> Result<u8, ParseError> {
        if self.bytes.len().saturating_sub(self.pos) < 2 {
            return Err(self.err_at(ErrorKind::InvalidHex, position));
        }

        let token = &self.bytes[self.pos..self.pos + 2];
        if !token.iter().all(u8::is_ascii_hexdigit) {
            return Err(self.err_at(ErrorKind::InvalidHex, position));
        }

        self.pos += 2;
        Ok(hex_nibble(token[0]) * 16 + hex_nibble(token[1]))
    }

    fn take_name(&mut self) -> &'a str {
        let start = self.pos;
        while matches!(self.peek(), Some(byte) if is_name_byte(byte)) {
            self.pos += 1;
        }
        &self.input[start..self.pos]
    }

    fn take_token(&mut self) -> Result<&'a str, ParseError> {
        let start = self.pos;
        while let Some(byte) = self.peek() {
            if byte == b'(' || byte == b')' || byte.is_ascii_whitespace() {
                break;
            }
            self.pos += 1;
        }

        if start == self.pos {
            Err(self.err(ErrorKind::UnexpectedEnd))
        } else {
            Ok(&self.input[start..self.pos])
        }
    }

    fn skip_spaces(&mut self) {
        while matches!(self.peek(), Some(byte) if byte.is_ascii_whitespace()) {
            self.pos += 1;
        }
    }

    fn consume(&mut self, expected: u8) -> bool {
        if self.peek() == Some(expected) {
            self.pos += 1;
            true
        } else {
            false
        }
    }

    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.pos).copied()
    }

    fn err(&self, kind: ErrorKind) -> ParseError {
        self.err_at(kind, self.pos)
    }

    fn err_at(&self, kind: ErrorKind, position: usize) -> ParseError {
        ParseError { position, kind }
    }
}

fn is_name_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn parse_hex_u16(token: &str) -> Option<u16> {
    if token.is_empty() || token.len() > 4 || !token.bytes().all(|byte| byte.is_ascii_hexdigit()) {
        return None;
    }
    u16::from_str_radix(token, 16).ok()
}

fn hex_nibble(byte: u8) -> u8 {
    match byte {
        b'0'..=b'9' => byte - b'0',
        b'a'..=b'f' => byte - b'a' + 10,
        b'A'..=b'F' => byte - b'A' + 10,
        _ => unreachable!("validated hex byte"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const BASIC_LCD: &str = include_str!("../fixtures/basic_lcd.caps");
    const COMPACT_VCP: &str = include_str!("../fixtures/compact_vcp.caps");
    const FALLBACK_CRT: &str = include_str!("../fixtures/fallback_crt.caps");
    const NESTED_VALUES: &str = include_str!("../fixtures/nested_values.caps");
    const DB_PATCH: &str = include_str!("../fixtures/db_patch_add_remove.caps");

    #[test]
    fn parses_basic_lcd_fixture() {
        let caps = Caps::parse(BASIC_LCD).unwrap();

        assert_eq!(caps.monitor_type(), MonitorType::Lcd);
        assert!(caps.is_supported(0x10));
        assert!(caps.is_supported(0x12));
        assert_eq!(
            caps.vcp(0x14).unwrap().values(),
            Some([0x05, 0x08, 0x0b].as_slice())
        );
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x01, 0x03].as_slice())
        );
    }

    #[test]
    fn parses_fallback_crt_fixture() {
        let caps = Caps::parse(FALLBACK_CRT).unwrap();

        assert_eq!(caps.monitor_type(), MonitorType::Crt);
        assert_eq!(
            caps.vcp_codes().collect::<Vec<_>>(),
            vec![0x10, 0x12, 0x16, 0x18, 0x1a, 0x50, 0x92]
        );
    }

    #[test]
    fn parses_compact_vcp_codes() {
        let caps = Caps::parse(COMPACT_VCP).unwrap();

        assert!(caps.is_supported(0x02));
        assert!(caps.is_supported(0x03));
        assert!(caps.is_supported(0x04));
        assert!(caps.is_supported(0x05));
        assert_eq!(
            caps.vcp(0x14).unwrap().values(),
            Some([0x05, 0x08].as_slice())
        );
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x03, 0x04].as_slice())
        );
    }

    #[test]
    fn tolerates_nested_value_groups() {
        let caps = Caps::parse(NESTED_VALUES).unwrap();

        assert_eq!(caps.vcp(0xe0).unwrap().values(), Some([0x02].as_slice()));
        assert_eq!(
            caps.vcp(0xe1).unwrap().values(),
            Some([0x03, 0x04].as_slice())
        );
        assert!(caps.is_supported(0xdf));
    }

    #[test]
    fn supports_database_style_add_remove() {
        let mut caps = Caps::parse(DB_PATCH).unwrap();

        assert!(caps.is_supported(0x10));
        assert!(caps.is_supported(0x12));
        assert_eq!(
            caps.vcp(0x60).unwrap().values(),
            Some([0x01, 0x03].as_slice())
        );

        assert_eq!(caps.apply_remove("(vcp(12 60(01)))").unwrap(), 2);
        assert!(!caps.is_supported(0x12));
        assert_eq!(caps.vcp(0x60).unwrap().values(), Some([0x03].as_slice()));

        assert_eq!(caps.apply_add("(vcp(12 60(0f)))").unwrap(), 2);
        assert!(caps.is_supported(0x12));
        assert_eq!(caps.vcp(0x60).unwrap().values(), Some([0x0f].as_slice()));
    }

    #[test]
    fn removing_specific_values_from_all_values_removes_control() {
        let mut caps = Caps::parse("(vcp(10))").unwrap();

        assert_eq!(caps.apply_remove("(vcp(10(01)))").unwrap(), 1);
        assert!(!caps.is_supported(0x10));
    }

    #[test]
    fn empty_value_list_matches_legacy_all_values_entry() {
        let mut caps = Caps::parse("(vcp(10()))").unwrap();

        assert_eq!(caps.vcp(0x10).unwrap().values(), None);
        assert_eq!(caps.apply_remove("(vcp(10()))").unwrap(), 1);
        assert!(!caps.is_supported(0x10));
    }

    #[test]
    fn accepts_uppercase_type_names() {
        assert_eq!(
            Caps::parse("(type(LCD))").unwrap().monitor_type(),
            MonitorType::Lcd
        );
        assert_eq!(
            Caps::parse("(type(CRT))").unwrap().monitor_type(),
            MonitorType::Crt
        );
    }

    #[test]
    fn unknown_type_does_not_override_existing_type() {
        let mut caps = Caps::parse("(type(lcd))").unwrap();

        assert_eq!(caps.apply_add("(type(OLED))").unwrap(), 0);
        assert_eq!(caps.monitor_type(), MonitorType::Lcd);
    }

    #[test]
    fn supports_mixed_controls_and_value_lists() {
        let caps = Caps::parse("(vcp(10(01 02) 12 14(0A)))").unwrap();

        assert_eq!(caps.vcp_codes().collect::<Vec<_>>(), vec![0x10, 0x12, 0x14]);
        assert_eq!(
            caps.vcp(0x10).unwrap().values(),
            Some([0x01, 0x02].as_slice())
        );
        assert_eq!(caps.vcp(0x12).unwrap().values(), None);
        assert_eq!(caps.vcp(0x14).unwrap().values(), Some([0x0a].as_slice()));
    }

    #[test]
    fn rejects_invalid_vcp_identifier() {
        assert_eq!(
            Caps::parse("(vcp(GG))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
        assert_eq!(
            Caps::parse("(vcp(-1))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
        assert_eq!(
            Caps::parse("(vcp(☃))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
    }

    #[test]
    fn rejects_invalid_values() {
        assert_eq!(
            Caps::parse("(vcp(10(0G)))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
        assert_eq!(
            Caps::parse("(vcp(10(10000)))").unwrap_err().kind(),
            ErrorKind::InvalidHex
        );
    }

    #[test]
    fn rejects_value_list_without_vcp_code() {
        assert_eq!(
            Caps::parse("(vcp((01)))").unwrap_err().kind(),
            ErrorKind::ValueWithoutVcpCode
        );
    }

    #[test]
    fn rejects_unbalanced_parentheses() {
        assert_eq!(
            Caps::parse("(vcp(10)").unwrap_err().kind(),
            ErrorKind::UnbalancedParens
        );
        assert_eq!(
            Caps::parse(")(vcp(10))").unwrap_err().kind(),
            ErrorKind::UnexpectedCloseParen
        );
    }

    #[test]
    fn accepts_nested_unknown_sections_before_vcp() {
        let caps = Caps::parse("(prot(monitor)foo(bar(baz))vcp(10))").unwrap();

        assert!(caps.is_supported(0x10));
    }

    #[test]
    fn supports_boundary_control_and_value_ranges() {
        let caps = Caps::parse("(vcp(00(0000) FF(FFFF)))").unwrap();

        assert_eq!(caps.vcp(0x00).unwrap().values(), Some([0x0000].as_slice()));
        assert_eq!(caps.vcp(0xff).unwrap().values(), Some([0xffff].as_slice()));
    }
}
