use super::*;
use std::mem::{align_of, size_of};

macro_rules! field_offset {
    ($ty:ty, $field:tt) => {{
        let value = std::mem::MaybeUninit::<$ty>::uninit();
        let base = value.as_ptr();
        unsafe { std::ptr::addr_of!((*base).$field) as usize - base as usize }
    }};
}

fn align_up(value: usize, align: usize) -> usize {
    (value + align - 1) & !(align - 1)
}

#[test]
fn monitor_database_ffi_layout_matches_c_abi_contract() {
    assert_eq!(size_of::<c_int>(), 4);
    assert_eq!(size_of::<c_uchar>(), 1);
    assert_eq!(size_of::<c_ushort>(), 2);

    assert_eq!(field_offset!(CValueDb, id), 0);
    assert_eq!(
        field_offset!(CValueDb, name),
        align_up(size_of::<*mut c_uchar>(), align_of::<*mut c_uchar>())
    );
    assert_eq!(
        field_offset!(CValueDb, value),
        size_of::<*mut c_uchar>() * 2
    );
    assert_eq!(field_offset!(CValueDbPrivate, public_value), 0);
    assert!(field_offset!(CValueDbPrivate, value16) >= size_of::<CValueDb>());

    assert_eq!(field_offset!(CControlDb, id), 0);
    assert_eq!(field_offset!(CControlDb, name), size_of::<*mut c_uchar>());
    assert!(field_offset!(CControlDb, address) > field_offset!(CControlDb, name));
    assert!(field_offset!(CControlDb, value_list) > field_offset!(CControlDb, next));

    assert_eq!(field_offset!(CSubgroupDb, name), 0);
    assert_eq!(
        field_offset!(CSubgroupDb, pattern),
        size_of::<*mut c_uchar>()
    );
    assert_eq!(field_offset!(CGroupDb, name), 0);
    assert_eq!(field_offset!(CMonitorDb, name), 0);
    assert_eq!(
        field_offset!(CMonitorDb, init),
        align_up(size_of::<*mut c_uchar>(), align_of::<c_int>())
    );
}

#[cfg(unix)]
#[test]
fn c_datadir_paths_preserve_non_utf8_bytes() {
    use std::ffi::CString;
    use std::os::unix::ffi::OsStrExt;

    let raw_path = b"/tmp/ddccontrol-\xff-db".to_vec();
    let c_path = CString::new(raw_path.clone()).unwrap();

    let path = unsafe { pathbuf_from_c_path(c_path.as_ptr()) };

    assert_eq!(path.as_os_str().as_bytes(), raw_path);
}

#[test]
fn c_bytes_preserves_non_utf8_labels() {
    let label = b"Contr\xf4le".to_vec();

    let ptr = unsafe { c_bytes(&label).unwrap() };
    let copied = unsafe { CStr::from_ptr(ptr as *const c_char).to_bytes().to_vec() };
    unsafe {
        free(ptr as *mut c_void);
    }

    assert_eq!(copied, label);
}

#[test]
fn decode_xml_bytes_uses_declared_non_utf8_encoding() {
    let xml =
        b"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><options name=\"Contr\xf4le\"/>";

    let decoded = decode_xml_bytes(xml);

    assert!(decoded.contains("Contr\u{00f4}le"));
}

#[test]
fn decode_xml_bytes_finds_encoding_after_leading_comment() {
    let xml = b"<!-- source -->\n\
        <?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\
        <options name=\"Contr\xf4le\"/>";

    let decoded = decode_xml_bytes(xml);

    assert!(decoded.contains("Contr\u{00f4}le"));
}

#[test]
fn normalize_xml_document_allows_comments_before_declaration() {
    let xml = "<!-- source -->\n<?xml version=\"1.0\"?><monitor/>".to_string();

    let normalized = normalize_xml_document(xml);

    assert!(normalized.starts_with("<?xml version=\"1.0\"?>"));
    Document::parse(&normalized).unwrap();
}

#[test]
fn parse_int_matches_strtol_style_database_values() {
    assert_eq!(parse_int(" 1").unwrap(), 1);
    assert_eq!(parse_int("+1").unwrap(), 1);
    assert_eq!(parse_int("-1").unwrap(), -1);
    assert_eq!(parse_int("0x10").unwrap(), 16);
    assert_eq!(parse_int("010").unwrap(), 8);
    assert!(parse_int("09").is_err());
    assert!(parse_int("1 ").is_err());
}

#[test]
fn unmatched_monitor_values_are_not_parsed() {
    let option_control = OptionControl {
        id: "input".to_string(),
        name: "Input".to_string(),
        control_type: CONTROL_TYPE_LIST,
        refresh: REFRESH_TYPE_NONE,
        values: vec![OptionValue {
            id: "hdmi".to_string(),
            name: Some("HDMI".to_string()),
        }],
    };
    let monitor_control = MonitorControl {
        id: "input".to_string(),
        raw_address: Some("0x60".to_string()),
        raw_delay: None,
        values: vec![
            MonitorValue {
                element_name: "value".to_string(),
                id: Some("hdmi".to_string()),
                raw_value: Some(" 1".to_string()),
                line: 1,
            },
            MonitorValue {
                element_name: "value".to_string(),
                id: Some("unused".to_string()),
                raw_value: Some("not-an-int".to_string()),
                line: 2,
            },
        ],
        child_index: 0,
    };

    let values = get_value_list(&option_control, &monitor_control, true).unwrap();

    assert_eq!(values.len(), 1);
    assert_eq!(values[0].id, "hdmi");
    assert_eq!(values[0].value16, 1);
}

#[test]
fn parse_monitor_controls_keeps_unknown_control_children_for_validation() {
    let doc = Document::parse(
        r#"<controls>
            <unknown id="bad"/>
            <control id="input" address="0x60"/>
        </controls>"#,
    )
    .unwrap();

    let parsed = parse_monitor_controls(doc.root_element()).unwrap();

    assert_eq!(parsed.elements.len(), 2);
    assert_eq!(parsed.elements[0].name, "unknown");
    assert_eq!(parsed.controls.len(), 1);
    assert_eq!(parsed.controls[0].child_index, 1);
}

#[test]
fn missing_value_id_is_deferred_until_control_is_matched() {
    let doc = Document::parse(
        r#"<controls>
            <control id="input" address="0x60">
                <value value="1"/>
            </control>
        </controls>"#,
    )
    .unwrap();

    let parsed = parse_monitor_controls(doc.root_element()).unwrap();

    assert_eq!(parsed.controls.len(), 1);
    assert!(parsed.controls[0].values[0].id.is_none());
}

#[test]
fn monitor_values_without_id_use_unmatched_validation() {
    let option_control = OptionControl {
        id: "input".to_string(),
        name: "Input".to_string(),
        control_type: CONTROL_TYPE_LIST,
        refresh: REFRESH_TYPE_NONE,
        values: vec![OptionValue {
            id: "hdmi".to_string(),
            name: Some("HDMI".to_string()),
        }],
    };
    let monitor_control = MonitorControl {
        id: "input".to_string(),
        raw_address: Some("0x60".to_string()),
        raw_delay: None,
        values: vec![MonitorValue {
            element_name: "value".to_string(),
            id: None,
            raw_value: Some("1".to_string()),
            line: 1,
        }],
        child_index: 0,
    };

    assert!(get_value_list(&option_control, &monitor_control, false).is_err());
    assert!(get_value_list(&option_control, &monitor_control, true).is_ok());
}

#[test]
fn parse_monitor_controls_keeps_control_without_id_unmatched() {
    let doc = Document::parse(
        r#"<controls>
            <control address="0x60"/>
        </controls>"#,
    )
    .unwrap();

    let parsed = parse_monitor_controls(doc.root_element()).unwrap();

    assert_eq!(parsed.elements.len(), 1);
    assert_eq!(parsed.elements[0].name, "control");
    assert!(parsed.elements[0].id.is_none());
    assert!(parsed.controls.is_empty());
}

#[test]
fn parse_monitor_controls_defers_address_and_delay_validation() {
    let doc = Document::parse(
        r#"<controls>
            <control id="unknown" address="not-hex" delay="bad"/>
        </controls>"#,
    )
    .unwrap();

    let parsed = parse_monitor_controls(doc.root_element()).unwrap();

    assert_eq!(parsed.controls.len(), 1);
    assert!(monitor_control_address(&parsed.controls[0]).is_err());
    assert!(monitor_control_delay(&parsed.controls[0]).is_err());
}

#[test]
fn unknown_monitor_value_children_use_unmatched_validation() {
    let option_control = OptionControl {
        id: "input".to_string(),
        name: "Input".to_string(),
        control_type: CONTROL_TYPE_LIST,
        refresh: REFRESH_TYPE_NONE,
        values: vec![OptionValue {
            id: "hdmi".to_string(),
            name: Some("HDMI".to_string()),
        }],
    };
    let monitor_control = MonitorControl {
        id: "input".to_string(),
        raw_address: Some("0x60".to_string()),
        raw_delay: None,
        values: vec![
            MonitorValue {
                element_name: "value".to_string(),
                id: Some("hdmi".to_string()),
                raw_value: Some("1".to_string()),
                line: 1,
            },
            MonitorValue {
                element_name: "unknown".to_string(),
                id: Some("extra".to_string()),
                raw_value: None,
                line: 2,
            },
        ],
        child_index: 0,
    };

    assert!(get_value_list(&option_control, &monitor_control, false).is_err());
    assert!(get_value_list(&option_control, &monitor_control, true).is_ok());
}
