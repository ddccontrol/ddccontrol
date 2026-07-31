use ddccontrol_caps::{Caps, ErrorKind, MonitorType};

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
fn accepts_nested_database_vcp_sections() {
    let caps = Caps::parse("type(lcd)vcp(vcp(10 14(01 05) C8 C9))").unwrap();

    assert_eq!(caps.monitor_type(), MonitorType::Lcd);
    assert_eq!(
        caps.vcp_codes().collect::<Vec<_>>(),
        vec![0x10, 0x14, 0xc8, 0xc9]
    );
    assert_eq!(
        caps.vcp(0x14).unwrap().values(),
        Some([0x01, 0x05].as_slice())
    );
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
        Caps::parse("(vcp(10").unwrap_err().kind(),
        ErrorKind::UnbalancedParens
    );
    assert_eq!(
        Caps::parse("(type(lcd)vcp(10 12)").unwrap_err().kind(),
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
