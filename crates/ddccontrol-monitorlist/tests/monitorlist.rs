use ddccontrol_monitorlist::{parse, parse_bytes, serialize, Monitor};

const VERSION: &str = "3.3.0";
const MONITOR: &str =
    r#"<monitor filename="dev:/dev/i2c-1" supported="1" name="Dell" digital="128"/>"#;

fn cache(children: &str) -> String {
    format!(r#"<monitorlist ddccontrolversion="{VERSION}">{children}</monitorlist>"#)
}

fn sample() -> Monitor {
    Monitor {
        filename: "dev:/dev/i2c-1".into(),
        supported: 1,
        name: "Dell".into(),
        digital: 128,
    }
}

#[test]
fn reads_legacy_cache_and_preserves_order_and_duplicates() {
    let xml = cache(&format!(
        "{MONITOR}<!-- comment --><?monitor ignored?>\n<unknown>{MONITOR}</unknown>\n{}\n{MONITOR}",
        MONITOR.replace("Dell", "Analog").replace("128", "0")
    ));
    let mut analog = sample();
    analog.name = "Analog".into();
    analog.digital = 0;
    assert_eq!(parse(&xml, VERSION).unwrap(), [sample(), analog, sample()]);
}

#[test]
fn ignores_unknown_attributes_and_matches_local_element_names() {
    let xml = cache(MONITOR).replace(
        "<monitor ",
        "<m:monitor xmlns:m=\"urn:test\" extra=\"ignored\" ",
    );
    assert_eq!(parse(&xml, VERSION).unwrap(), [sample()]);
}

#[test]
fn accepts_empty_lists_and_empty_string_attributes() {
    assert!(parse(&cache(""), VERSION).unwrap().is_empty());
    assert!(parse(&serialize(&[], VERSION).unwrap(), VERSION)
        .unwrap()
        .is_empty());
    let monitors = parse(
        &cache(&MONITOR.replace("Dell", "").replace("dev:/dev/i2c-1", "")),
        VERSION,
    )
    .unwrap();
    assert_eq!(monitors[0].name, "");
    assert_eq!(monitors[0].filename, "");
}

#[test]
fn rejects_wrong_root_version_and_missing_attributes() {
    for xml in [
        "",
        "<profile/>",
        "<monitorlist/>",
        r#"<monitorlist ddccontrolversion="3.3.1"/>"#,
    ] {
        assert!(parse(xml, VERSION).is_err(), "{xml}");
    }
    for (name, value) in [
        ("filename", "dev:/dev/i2c-1"),
        ("supported", "1"),
        ("name", "Dell"),
        ("digital", "128"),
    ] {
        let incomplete = MONITOR.replace(&format!(" {name}=\"{value}\""), "");
        // A valid first monitor must not hide an invalid later entry.
        assert!(
            parse(&cache(&format!("{MONITOR}{incomplete}")), VERSION).is_err(),
            "{name}"
        );
    }
}

#[test]
fn rejects_malformed_xml_and_dtds() {
    for xml in [
        cache(MONITOR).replace("</monitorlist>", ""),
        cache(MONITOR).replace("Dell", "&unknown;"),
        cache(MONITOR).replace("Dell", "&#0;"),
        cache(MONITOR).replace("Dell", "bad\u{1}name"),
        cache(MONITOR).replace("name=", "digital=\"0\" name="),
        format!("{}{}", cache(MONITOR), cache(MONITOR)),
        format!(
            "<!DOCTYPE monitorlist [<!ENTITY name 'Dell'>]>{}",
            cache(&MONITOR.replace("Dell", "&name;"))
        ),
        format!(
            "<!DOCTYPE monitorlist SYSTEM 'file:///does-not-exist'>{}",
            cache(MONITOR)
        ),
    ] {
        assert!(parse(&xml, VERSION).is_err(), "{xml:?}");
    }
}

#[test]
fn parses_base_zero_flags_without_treating_digital_as_boolean() {
    for (input, expected) in [
        ("0", 0),
        ("1", 1),
        ("128", 128),
        ("0200", 128),
        ("0x80", 128),
        ("0Xff", 255),
        (" +0377", 255),
    ] {
        let xml = cache(
            &MONITOR
                .replace("supported=\"1\"", &format!("supported=\"{input}\""))
                .replace("digital=\"128\"", &format!("digital=\"{input}\"")),
        );
        let result = parse(&xml, VERSION).unwrap();
        assert_eq!(result[0].supported, expected);
        assert_eq!(result[0].digital, expected);
    }
}

#[test]
fn rejects_invalid_or_truncating_flag_values() {
    for value in [
        "",
        " ",
        "-1",
        "256",
        "0x100",
        "0400",
        "18446744073709551616",
        "128 ",
        "08",
        "0x",
        "++1",
        "0x+1",
        "00+1",
        "1junk",
    ] {
        for (field, old) in [("supported", "1"), ("digital", "128")] {
            let xml = cache(&MONITOR.replace(
                &format!("{field}=\"{old}\""),
                &format!("{field}=\"{value}\""),
            ));
            assert!(parse(&xml, VERSION).is_err(), "{field}={value:?}");
        }
    }
}

#[test]
fn round_trips_all_byte_flags_and_xml_attribute_characters() {
    let monitors: Vec<_> = (0..=255)
        .map(|value| Monitor {
            filename: "dev:<&>\"'\n\r\t".into(),
            supported: value,
            name: "Skjerm Æøå 日本語 🖥 <&>\"'\n\r\t".into(),
            digital: 255 - value,
        })
        .collect();
    let version = "test<&>\"'\n\r\t";
    let xml = serialize(&monitors, version).unwrap();
    assert_eq!(parse_bytes(xml.as_bytes(), version).unwrap(), monitors);
    assert!(xml.contains("digital=\"128\""));
}

#[test]
fn refuses_to_serialize_invalid_xml_characters_in_any_string() {
    for invalid in ["\0", "\u{1}", "\u{B}", "\u{FFFE}", "\u{FFFF}"] {
        assert!(serialize(&[], invalid).is_err());
        let mut monitor = sample();
        monitor.filename = invalid.into();
        assert!(serialize(&[monitor], VERSION).is_err());
        let mut monitor = sample();
        monitor.name = invalid.into();
        assert!(serialize(&[monitor], VERSION).is_err());
    }
}

#[test]
fn decodes_utf8_bom_and_utf16_in_both_byte_orders() {
    let xml = serialize(&[sample()], VERSION)
        .unwrap()
        .replace("UTF-8", "UTF-16");
    for big_endian in [false, true] {
        let bytes: Vec<_> = xml
            .encode_utf16()
            .flat_map(|unit| {
                if big_endian {
                    unit.to_be_bytes()
                } else {
                    unit.to_le_bytes()
                }
            })
            .collect();
        assert_eq!(parse_bytes(&bytes, VERSION).unwrap(), [sample()]);
        let mut with_bom = if big_endian {
            vec![0xfe, 0xff]
        } else {
            vec![0xff, 0xfe]
        };
        with_bom.extend_from_slice(&bytes);
        assert_eq!(parse_bytes(&with_bom, VERSION).unwrap(), [sample()]);
    }
    let mut utf8 = vec![0xef, 0xbb, 0xbf];
    utf8.extend_from_slice(cache(MONITOR).as_bytes());
    assert_eq!(parse_bytes(&utf8, VERSION).unwrap(), [sample()]);
}

#[test]
fn decodes_declared_legacy_encoding_and_rejects_bad_bytes() {
    for whitespace in [" ", "\n", "\t"] {
        let mut bytes = format!(
            "<?xml{whitespace}version='1.0' encoding = 'ISO-8859-1'?>{}",
            cache(&MONITOR.replace("Dell", "Caf!"))
        )
        .into_bytes();
        let index = bytes.iter().position(|byte| *byte == b'!').unwrap();
        bytes[index] = 0xe9;
        assert_eq!(parse_bytes(&bytes, VERSION).unwrap()[0].name, "Café");
    }
    let mut bad_utf8 = cache(MONITOR).into_bytes();
    bad_utf8[0] = 0xff;
    assert!(parse_bytes(&bad_utf8, VERSION).is_err());
    assert!(parse_bytes(b"\xff\xfe<", VERSION).is_err());
    for declaration in [
        "<?xml version='1.0' encoding='unknown'?>",
        "<?xml version='1.0' encoding=UTF-8?>",
        "<?xml version='1.0' encoding='UTF-8'",
    ] {
        let xml = format!("{declaration}{}", cache(MONITOR));
        assert!(parse_bytes(xml.as_bytes(), VERSION).is_err());
        let mut with_bom = vec![0xef, 0xbb, 0xbf];
        with_bom.extend_from_slice(xml.as_bytes());
        assert!(parse_bytes(&with_bom, VERSION).is_err());
    }
}
