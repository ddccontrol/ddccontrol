// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use ddccontrol_profile::{parse, parse_bytes, serialize, Control, Profile, MAX_CONTROLS};

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
        name: "Work & \"Play\" <HDR>\nSecond\tcolumn\rreturn".to_string(),
        pnp_id: "DEL'1234".to_string(),
        controls: vec![Control {
            address: 0x10,
            value: 0x1234,
        }],
    };

    let xml = serialize(&profile).unwrap();
    assert!(xml.contains("Work &amp; &quot;Play&quot; &lt;HDR&gt;"));
    assert!(xml.contains("DEL&apos;1234"));
    assert!(xml.contains("&#xA;Second&#x9;column&#xD;return"));
    assert_eq!(parse(&xml).unwrap(), profile);
}

#[test]
fn rejects_characters_that_are_invalid_in_xml() {
    let profile = Profile {
        name: "Invalid \u{1} name".to_string(),
        pnp_id: "DEL1234".to_string(),
        controls: vec![],
    };

    assert!(serialize(&profile).is_err());
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
fn rejects_invalid_bytes_and_unsupported_declared_encodings() {
    assert!(parse_bytes(
        b"<?xml version=\"1.0\" encoding=\"UTF-8\"?><profile name=\"\xff\" pnpid=\"DEL1234\" version=\"1\"/>"
    )
    .is_err());

    let padding = " ".repeat(300);
    let xml = format!(
        "<?xml version=\"1.0\"{padding}encoding=\"X-UNKNOWN\"?><profile name=\"Office\" pnpid=\"DEL1234\" version=\"1\"/>"
    );
    assert!(parse_bytes(xml.as_bytes()).is_err());
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
