//! Independent fixtures were committed before this Rust implementation existed.
use ddccontrol_db_format::{
    self as format, array, encode, field, map, optional, uint, validate, value, Value,
};
use ddccontrol_dbgen::{convert, diagnostic, parse_xml, source_integer, METADATA};
use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
    sync::atomic::{AtomicUsize, Ordering},
};

const BASE: &[u8] = include_bytes!("../../ddccontrol-db/fixtures/cbor/reference-v1.cbor");
const NEWER: &[u8] = include_bytes!("../../ddccontrol-db/fixtures/cbor/newer-v1.cbor");
const DESCRIPTIONS: &[u8] =
    include_bytes!("../../ddccontrol-db/fixtures/cbor/descriptions-v1.cbor");
const BIN: &str = env!("CARGO_BIN_EXE_ddccontrol-dbgen");
fn fixtures() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures")
}
fn obj(values: Vec<(u64, Value)>) -> Value {
    Value::Map(values.into_iter().map(|(k, v)| (k.into(), v)).collect())
}
fn set(v: &mut Value, key: u64, new: Value) {
    let entries = v.as_map_mut().unwrap();
    if let Some((_, value)) = entries.iter_mut().find(|(k, _)| uint(k) == Ok(key)) {
        *value = new;
    } else {
        entries.push((key.into(), new));
    }
}
fn at(v: &mut Value, key: u64) -> &mut Value {
    &mut v
        .as_map_mut()
        .unwrap()
        .iter_mut()
        .find(|(k, _)| uint(k) == Ok(key))
        .unwrap()
        .1
}
fn profile<'a>(v: &'a mut Value, id: &str) -> &'a mut Value {
    &mut at(v, 6)
        .as_map_mut()
        .unwrap()
        .iter_mut()
        .find(|(k, _)| k.as_text() == Some(id))
        .unwrap()
        .1
}
fn child(v: &Value, index: usize) -> &Value {
    &array(field(v, 2).unwrap()).unwrap()[index]
}
fn extension(id: &str, required: bool, payload: Value) -> Value {
    obj(vec![(0, id.into()), (1, required.into()), (2, payload)])
}
struct Temp(PathBuf);
impl Temp {
    fn new() -> Self {
        static NEXT: AtomicUsize = AtomicUsize::new(0);
        let path = std::env::temp_dir().join(format!(
            "ddccontrol-dbgen-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        Self(path)
    }
    fn source(&self) -> PathBuf {
        let path = self.0.join("source");
        fs::create_dir_all(path.join("monitor")).unwrap();
        // distcheck makes source fixtures read-only. Temporary copies must be
        // writable for malformed-source tests without changing fixture modes.
        fs::write(
            path.join("options.xml"),
            fs::read(fixtures().join("source/options.xml")).unwrap(),
        )
        .unwrap();
        for id in ["VESA", "TST0001"] {
            fs::write(
                path.join(format!("monitor/{id}.xml")),
                fs::read(fixtures().join(format!("source/monitor/{id}.xml"))).unwrap(),
            )
            .unwrap();
        }
        path
    }
}
impl Drop for Temp {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

#[test]
fn frozen_files_encode_and_json_are_exact() {
    let root = convert(&fixtures().join("source"), None).unwrap();
    assert_eq!(encode(&root).unwrap(), BASE);
    assert_eq!(
        diagnostic(&root).unwrap(),
        include_str!("../../ddccontrol-db/fixtures/cbor/reference-v1.json")
    );
    assert_eq!(
        encode(&format::fallback_manifest(&root).unwrap()).unwrap(),
        include_bytes!("../fixtures/base-v1.snapshot")
    );
    for (wire, json) in [
        (
            NEWER,
            include_str!("../../ddccontrol-db/fixtures/cbor/newer-v1.json"),
        ),
        (
            DESCRIPTIONS,
            include_str!("../../ddccontrol-db/fixtures/cbor/descriptions-v1.json"),
        ),
    ] {
        assert_eq!(encode(&validate(wire).unwrap()).unwrap(), wire);
        assert_eq!(diagnostic(&validate(wire).unwrap()).unwrap(), json);
    }
}

#[test]
fn independent_rfc_vectors_and_integer_extremes() {
    for (value, wire) in [
        (0u64.into(), vec![0]),
        (23u64.into(), vec![23]),
        (24u64.into(), vec![0x18, 24]),
        (255u64.into(), vec![0x18, 255]),
        (256u64.into(), vec![0x19, 1, 0]),
        (65535u64.into(), vec![0x19, 255, 255]),
        (65536u64.into(), vec![0x1a, 0, 1, 0, 0]),
        (
            u64::MAX.into(),
            vec![0x1b, 255, 255, 255, 255, 255, 255, 255, 255],
        ),
        (
            Value::Integer((-18446744073709551616i128).try_into().unwrap()),
            vec![0x3b, 255, 255, 255, 255, 255, 255, 255, 255],
        ),
        ((-1i64).into(), vec![0x20]),
        (Value::Bytes(vec![255, 0]), vec![0x42, 255, 0]),
        ("é".into(), vec![0x62, 0xc3, 0xa9]),
        (
            Value::Array(vec![false.into(), true.into(), Value::Null]),
            vec![0x83, 0xf4, 0xf5, 0xf6],
        ),
        (
            Value::Map(vec![(24u64.into(), 0.into()), ("".into(), 0.into())]),
            vec![0xa2, 0x18, 24, 0, 0x60, 0],
        ),
    ] {
        assert_eq!(encode(&value).unwrap(), wire);
        assert_eq!(format::value(&wire).unwrap(), value);
    }
    let extreme = Value::Integer((-18446744073709551616i128).try_into().unwrap());
    assert_eq!(diagnostic(&extreme).unwrap(), "-18446744073709551616\n");
    let ambiguous = Value::Map(vec![
        (1.into(), Value::Bytes(vec![1])),
        ("1".into(), Value::Map(vec![("$bytes".into(), "01".into())])),
    ]);
    let dump = diagnostic(&ambiguous).unwrap();
    assert!(dump.contains("\"$map\""));
    assert!(dump.contains("\"$bytes\": \"01\""));
}

#[test]
fn malformed_input_and_all_truncations_are_rejected() {
    for bad in [
        vec![],
        vec![0x1a, 0, 0],
        vec![0, 0],
        vec![0xa2, 0, 0, 0, 1],
        vec![0xa2, 1, 0, 0, 0],
        vec![0x61, 255],
        vec![0xc0, 0],
        vec![0xfa, 0, 0, 0, 0],
        vec![0xc2, 0x41, 1],
        vec![0x9f, 0xff],
        vec![0x18, 0],
        vec![0xf7],
        vec![0xa1, 0x20, 0],
        vec![0xa1, 0xf5, 0],
        vec![0x1c],
        vec![0x9b, 255, 255, 255, 255, 255, 255, 255, 255],
        vec![0x7b, 255, 255, 255, 255, 255, 255, 255, 255],
    ] {
        assert!(format::value(&bad).is_err(), "{bad:?}");
    }
    let mut deep = vec![0x81; 65];
    deep.push(0);
    assert!(format::value(&deep).is_err());
    for end in 0..BASE.len() {
        assert!(validate(&BASE[..end]).is_err(), "prefix {end}");
    }
    assert!(encode(&Value::Map(vec![
        (0.into(), 0.into()),
        (0.into(), 1.into())
    ]))
    .is_err());
    assert!(encode(&Value::Bytes(vec![0; format::MAX_BYTES + 1])).is_err());
    assert!(encode(&Value::Text("x".repeat(format::MAX_TEXT + 1))).is_err());
}

#[test]
fn locale_working_directory_revision_and_cli_help() {
    let temp = Temp::new();
    for locale in ["C", "C.UTF-8", "nb_NO.UTF-8", "fr_FR.UTF-8"] {
        let output = Command::new(BIN)
            .args(["convert"])
            .arg(fixtures().join("source"))
            .arg(temp.0.join("out.cbor"))
            .env("LC_ALL", locale)
            .env("LANG", locale)
            .current_dir("/")
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert_eq!(fs::read(temp.0.join("out.cbor")).unwrap(), BASE);
    }
    let root = convert(&fixtures().join("source"), Some("chosen-revision")).unwrap();
    assert_eq!(field(&root, 2).unwrap().as_text(), Some("chosen-revision"));
    assert!(convert(&fixtures().join("source"), Some("")).is_err());
    assert!(Command::new(BIN)
        .arg("--help")
        .output()
        .unwrap()
        .status
        .success());
    assert_eq!(
        Command::new(BIN).arg("--version").output().unwrap().stdout,
        b"ddccontrol-dbgen 0.1.0\n"
    );
}

#[test]
fn legacy_encodings_comments_zero_missing_duplicates_and_inert_metadata() {
    let root = parse_xml(b"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><monitor name=\"\xe9\"><controls><control id=\"x\" address=\"0\" delay=\"0\" default=\"12\"/><control id=\"x\" address=\"16\"/></controls><!--comment--><future flag=\"1\"/></monitor>","monitor").unwrap();
    assert_eq!(
        field(field(&root, 1).unwrap(), 0).unwrap().as_text(),
        Some("é")
    );
    let first = child(child(&root, 0), 0);
    let second = child(child(&root, 0), 1);
    assert_eq!(
        uint(field(field(first, 1).unwrap(), 6).unwrap()).unwrap(),
        0
    );
    assert_eq!(
        uint(field(field(first, 1).unwrap(), 7).unwrap()).unwrap(),
        0
    );
    assert!(optional(field(second, 1).unwrap(), 7).is_none());
    assert_eq!(array(field(&root, 2).unwrap()).unwrap().len(), 2);
    assert_eq!(uint(field(child(&root, 1), 0).unwrap()).unwrap(), 255);
    let metadata = field(&array(field(first, 3).unwrap()).unwrap()[0], 2).unwrap();
    assert_eq!(
        map(field(metadata, 0).unwrap()).unwrap(),
        &[("default".into(), "12".into())]
    );
    let prefix = parse_xml(b" \n<!-- old prefix -->\n<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><monitor name=\"\x80cran\"/>","monitor").unwrap();
    assert_eq!(
        field(field(&prefix, 1).unwrap(), 0).unwrap().as_text(),
        Some("€cran")
    );
    assert!(parse_xml(
        b"<?xml version=\"1.0\" encoding=\"unknown-encoding\"?><monitor/>",
        "monitor"
    )
    .unwrap_err()
    .contains("unsupported source XML encoding"));
    assert!(parse_xml(b"<monitor name=\"\xff\"/>", "monitor").is_err());
}

#[test]
fn utf16_both_byte_orders_and_entities() {
    let xml = "<?xml version=\"1.0\" encoding=\"UTF-16\"?><monitor name=\"Écran\"/>";
    for little in [true, false] {
        let mut bytes = if little {
            vec![255, 254]
        } else {
            vec![254, 255]
        };
        for unit in xml.encode_utf16() {
            bytes.extend(if little {
                unit.to_le_bytes()
            } else {
                unit.to_be_bytes()
            });
        }
        let root = parse_xml(&bytes, "monitor").unwrap();
        assert_eq!(
            field(field(&root, 1).unwrap(), 0).unwrap().as_text(),
            Some("Écran")
        );
    }
    for xml in [
        "<!DOCTYPE monitor><monitor/>",
        "<!DOCTYPE monitor [<!ENTITY label 'name'>]><monitor name='&label;'/>",
    ] {
        assert!(parse_xml(xml.as_bytes(), "monitor").is_err());
    }
}

#[test]
fn namespaces_and_inactive_branches_never_activate_attributes() {
    let root = parse_xml(br#"<n:monitor xmlns:n="urn:example:xml" name="Generic"><control address="invalid"><value value="invalid"/></control><future><monitor><include file="MISSING"/><controls><control address="invalid"/></controls></monitor></future><n:controls><n:control id="x" address="16" n:address="invalid"><n:value id="wide" value="65535" n:value="invalid"/></n:control></n:controls></n:monitor>"#,"monitor").unwrap();
    for node in [
        child(&root, 0),
        child(child(&root, 0), 0),
        child(child(&root, 1), 0),
        child(child(child(&root, 1), 0), 0),
    ] {
        assert!(map(field(node, 1).unwrap()).unwrap().is_empty());
    }
    let active = child(child(&root, 2), 0);
    assert_eq!(
        uint(field(field(active, 1).unwrap(), 6).unwrap()).unwrap(),
        16
    );
    assert_eq!(
        uint(field(field(child(active, 0), 1).unwrap(), 8).unwrap()).unwrap(),
        65535
    );
    let metadata = field(&array(field(active, 3).unwrap()).unwrap()[0], 2).unwrap();
    assert_eq!(
        map(field(metadata, 0).unwrap()).unwrap(),
        &[("{urn:example:xml}address".into(), "invalid".into())]
    );
}

#[test]
fn non_whitespace_text_tail_and_comments_are_preserved_correctly() {
    let root = parse_xml(b"<monitor>first<!-- omitted -->second<future/>tail<!-- omitted -->end<controls/></monitor>","monitor").unwrap();
    let metadata = field(&array(field(&root, 3).unwrap()).unwrap()[0], 2).unwrap();
    assert_eq!(field(metadata, 2).unwrap().as_text(), Some("firstsecond"));
    let metadata = field(&array(field(child(&root, 0), 3).unwrap()).unwrap()[0], 2).unwrap();
    assert_eq!(field(metadata, 3).unwrap().as_text(), Some("tailend"));
}

#[test]
fn numeric_grammar_ranges_and_long_literals() {
    for (raw, name, expected) in [
        ("020", "address", 16),
        ("+0x10", "address", 16),
        ("  +016", "address", 14),
        ("000", "address", 0),
        ("020", "delay", 20),
        (" -10", "delay", -10),
        ("65535", "value", 65535),
        ("-2147483648", "delay", -2147483648),
    ] {
        assert_eq!(source_integer(raw, name).unwrap(), Value::from(expected));
    }
    for (raw, name) in [
        ("08", "address"),
        ("16 ", "address"),
        ("0x10", "delay"),
        ("", "value"),
        ("1_0", "value"),
        ("\u{a0}16", "address"),
        ("256", "address"),
        ("-1", "address"),
        ("65536", "value"),
        ("2147483648", "delay"),
        ("2", "dbversion"),
    ] {
        assert!(source_integer(raw, name).is_err(), "{raw} {name}");
    }
    for name in ["delay", "address", "value"] {
        assert_eq!(source_integer(&"0".repeat(5000), name).unwrap(), 0.into());
        assert_eq!(
            source_integer(&("0".repeat(5000) + "1"), name).unwrap(),
            1.into()
        );
        assert!(source_integer(&"1".repeat(5000), name)
            .unwrap_err()
            .contains("outside normative range"));
    }
}

#[test]
fn newer_database_preserves_old_profiles_and_open_numeric_codes() {
    let base = validate(BASE).unwrap();
    let newer = validate(NEWER).unwrap();
    let mut produced = convert(&fixtures().join("newer-source"), None).unwrap();
    set(&mut produced, 10, field(&newer, 10).unwrap().clone());
    let newer_profile = map(field(&newer, 6).unwrap())
        .unwrap()
        .iter()
        .find(|(k, _)| k.as_text() == Some("NEW0001"))
        .unwrap()
        .1
        .clone();
    let target = profile(&mut produced, "NEW0001");
    set(target, 4, field(&newer_profile, 4).unwrap().clone());
    let control = &mut at(&mut at(target, 2).as_array_mut().unwrap()[1], 2)
        .as_array_mut()
        .unwrap()[0];
    set(
        control,
        3,
        field(child(child(&newer_profile, 1), 0), 3)
            .unwrap()
            .clone(),
    );
    assert_eq!(encode(&produced).unwrap(), NEWER);
    for id in ["VESA", "TST0001"] {
        assert_eq!(
            profile(&mut base.clone(), id),
            profile(&mut newer.clone(), id)
        );
    }
    assert_eq!(
        encode(&format::fallback_manifest(&newer).unwrap()).unwrap(),
        include_bytes!("../fixtures/newer-v1.snapshot")
    );
}

#[test]
fn descriptor_validation_and_nested_necessity() {
    let description = "https://ddccontrol.sourceforge.net/cbor/ext/function-description/1";
    for (key, value) in [
        (0, 7.into()),
        (1, 4.into()),
        (4, 256.into()),
        (
            8,
            obj(vec![
                (0, 1.into()),
                (1, obj(vec![(0, 1.into()), (1, 0.into())])),
            ]),
        ),
        (9, obj(vec![(0, 2.into()), (1, 12.into())])),
        (
            11,
            Value::Array(vec![obj(vec![(0, 0.into()), (1, 65.into())])]),
        ),
        (
            16,
            Value::Array(vec![obj(vec![
                (0, "urn:a".into()),
                (1, "urn:b".into()),
                (2, "not bytes".into()),
            ])]),
        ),
    ] {
        let mut root = format::value(DESCRIPTIONS).unwrap();
        let ext = &mut at(profile(&mut root, "TST0001"), 3).as_array_mut().unwrap()[0];
        set(at(ext, 2), key, value);
        assert!(
            validate(&encode(&root).unwrap()).is_err(),
            "descriptor {key}"
        );
    }
    let mut root = value(BASE).unwrap();
    set(
        &mut root,
        7,
        Value::Array(vec![extension(
            description,
            false,
            obj(vec![(
                18,
                Value::Array(vec![extension("urn:example:future", true, Value::Null)]),
            )]),
        )]),
    );
    let root = validate(&encode(&root).unwrap()).unwrap();
    assert_eq!(format::fallback_manifest(&root).unwrap(), false.into());
}

#[test]
fn metadata_duplicates_and_identity_grammar_share_reader_rules() {
    let description = "https://ddccontrol.sourceforge.net/cbor/ext/function-description/1";
    let mut bad = vec![
        extension(METADATA, true, obj(vec![(0, Value::Map(vec![]))])),
        extension(METADATA, false, obj(vec![])),
        extension(
            METADATA,
            false,
            obj(vec![(0, Value::Map(vec![("x".into(), 3.into())]))]),
        ),
    ];
    for id in [
        "urn:bad identity",
        "urn:x:\n",
        "urn:x:\0",
        "urn:x:\u{7f}",
        "urn:x:\u{80}",
        "urn:x:\u{a0}",
        "urn:x:\u{2003}",
        "relative",
        "1scheme:value",
        "",
    ] {
        bad.push(extension(id, false, Value::Null));
        bad.push(extension(description, false, obj(vec![(3, id.into())])));
        bad.push(extension(
            description,
            false,
            obj(vec![(
                18,
                Value::Array(vec![extension(id, false, Value::Null)]),
            )]),
        ));
    }
    for ext in bad {
        let mut root = value(BASE).unwrap();
        set(&mut root, 7, Value::Array(vec![ext]));
        assert!(validate(&encode(&root).unwrap()).is_err());
    }
    let mut root = value(BASE).unwrap();
    let ext = extension("urn:example:future", false, Value::Null);
    set(&mut root, 7, Value::Array(vec![ext.clone(), ext]));
    assert!(validate(&encode(&root).unwrap()).is_err());
    for id in [
        "urn:example:feature:1",
        "https://example.org/feature%20name/1",
        "SCHEME+name.test-1:payload",
    ] {
        set(
            &mut root,
            7,
            Value::Array(vec![
                extension(id, false, Value::Null),
                extension(description, false, obj(vec![(3, id.into())])),
            ]),
        );
        validate(&encode(&root).unwrap()).unwrap();
    }
}

#[test]
fn unknown_required_and_optional_fields_survive_rewrite_and_guard_fallback() {
    let temp = Temp::new();
    for unknown_kind in [None, Some(9), Some(254), Some(256), Some(u64::MAX)] {
        let mut root = value(BASE).unwrap();
        set(
            &mut root,
            1000,
            Value::Map(vec![("future".into(), Value::Bytes(vec![0, 255]))]),
        );
        set(
            profile(&mut root, "VESA"),
            1001,
            "optional inert data".into(),
        );
        if let Some(kind) = unknown_kind {
            at(profile(&mut root, "TST0001"), 2)
                .as_array_mut()
                .unwrap()
                .push(obj(vec![
                    (0, kind.into()),
                    (1, Value::Map(vec![])),
                    (2, Value::Array(vec![])),
                ]));
        } else {
            set(
                profile(&mut root, "TST0001"),
                3,
                Value::Array(vec![extension(
                    "https://example.org/matcher/1",
                    true,
                    obj(vec![(0, 99.into())]),
                )]),
            );
        }
        let wire = encode(&root).unwrap();
        validate(&wire).unwrap();
        fs::write(temp.0.join("in.cbor"), &wire).unwrap();
        let output = Command::new(BIN)
            .arg("rewrite")
            .arg(temp.0.join("in.cbor"))
            .arg(temp.0.join("out.cbor"))
            .arg("--snapshot")
            .arg(temp.0.join("out.snapshot"))
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert_eq!(fs::read(temp.0.join("out.cbor")).unwrap(), wire);
        assert_eq!(fs::read(temp.0.join("out.snapshot")).unwrap(), b"\xf4");
    }
}

#[test]
fn inert_unknown_source_and_node_shaped_payloads_keep_xml_fallback() {
    let mut root = value(BASE).unwrap();
    let unknown = obj(vec![
        (0, 9.into()),
        (1, Value::Map(vec![])),
        (2, Value::Array(vec![])),
    ]);
    set(
        &mut root,
        7,
        Value::Array(vec![extension(
            "urn:example:description",
            false,
            unknown.clone(),
        )]),
    );
    set(&mut root, 100, unknown);
    at(profile(&mut root, "TST0001"), 2)
        .as_array_mut()
        .unwrap()
        .push(obj(vec![
            (0, 255.into()),
            (1, Value::Map(vec![])),
            (2, Value::Array(vec![])),
            (
                3,
                Value::Array(vec![extension(
                    METADATA,
                    false,
                    obj(vec![(0, Value::Map(vec![])), (1, "inert-source".into())]),
                )]),
            ),
        ]));
    let root = validate(&encode(&root).unwrap()).unwrap();
    assert_eq!(
        &format::fallback_manifest(&root).unwrap(),
        field(&root, 8).unwrap()
    );
}

#[test]
fn source_snapshot_and_manifest_failures() {
    let mut root = value(BASE).unwrap();
    at(&mut root, 8).as_map_mut().unwrap()[0].1 = Value::Bytes(vec![0; 32]);
    assert!(validate(&encode(&root).unwrap()).is_err());
    let mut root = value(BASE).unwrap();
    at(&mut root, 8).as_map_mut().unwrap()[0].0 = "../escape".into();
    let digest =
        format::manifest_digest(&value(&encode(field(&root, 8).unwrap()).unwrap()).unwrap())
            .unwrap();
    set(&mut root, 9, Value::Bytes(digest));
    assert!(validate(&encode(&root).unwrap()).is_err());
    let temp = Temp::new();
    let source = temp.source();
    fs::remove_file(source.join("options.xml")).unwrap();
    assert!(convert(&source, None).is_err());
}

#[test]
fn invalid_xml_profile_filenames_cannot_disappear_from_the_snapshot() {
    let temp = Temp::new();
    let source = temp.source();
    let invalid = source.join("monitor/.xml");
    fs::write(&invalid, "<monitor/>").unwrap();
    assert!(convert(&source, None)
        .unwrap_err()
        .contains("unsafe profile identifier"));
    fs::remove_file(&invalid).unwrap();

    #[cfg(unix)]
    {
        use std::os::unix::ffi::OsStringExt;
        let invalid = source
            .join("monitor")
            .join(std::ffi::OsString::from_vec(b"bad\xff.xml".to_vec()));
        fs::write(invalid, "<monitor/>").unwrap();
        assert!(convert(&source, None)
            .unwrap_err()
            .contains("source filename is not UTF-8"));
    }
}

#[test]
fn missing_references_cycles_depth_and_expansion_are_rejected() {
    let temp = Temp::new();
    let source = temp.source();
    for target in ["MISSING", "TST0001"] {
        fs::write(
            source.join("monitor/VESA.xml"),
            format!("<monitor><include file=\"{target}\"/></monitor>"),
        )
        .unwrap();
        assert!(convert(&source, None).is_err());
    }
    // 21 duplicating levels expand beyond the million-visit bound without
    // requiring a large file or unbounded recursive traversal.
    for index in 0..=21 {
        let body = if index == 21 {
            String::new()
        } else {
            format!(
                "<include file=\"P{}\"/><include file=\"P{}\"/>",
                index + 1,
                index + 1
            )
        };
        fs::write(
            source.join(format!("monitor/P{index}.xml")),
            format!("<monitor>{body}</monitor>"),
        )
        .unwrap();
    }
    fs::write(source.join("monitor/VESA.xml"), "<monitor/>").unwrap();
    assert!(convert(&source, None)
        .unwrap_err()
        .contains("excessive include"));
}

#[test]
fn failed_generation_removes_stale_artifacts_and_path_aliases_are_safe() {
    let temp = Temp::new();
    let source = temp.source();
    let output = temp.0.join("out.cbor");
    let snapshot = temp.0.join("out.snapshot");
    for path in [&output, &snapshot] {
        fs::write(path, b"old").unwrap();
    }
    fs::write(source.join("monitor/VESA.xml"), "<broken").unwrap();
    let result = Command::new(BIN)
        .arg("convert")
        .arg(&source)
        .arg(&output)
        .arg("--snapshot")
        .arg(&snapshot)
        .output()
        .unwrap();
    assert!(!result.status.success());
    assert!(!output.exists());
    assert!(!snapshot.exists());
    let original = fs::read(source.join("options.xml")).unwrap();
    let result = Command::new(BIN)
        .arg("convert")
        .arg(&source)
        .arg(source.join("options.xml"))
        .output()
        .unwrap();
    assert!(!result.status.success());
    assert_eq!(fs::read(source.join("options.xml")).unwrap(), original);
    let result = Command::new(BIN)
        .arg("convert")
        .arg(&source)
        .arg("out.cbor")
        .arg("--snapshot")
        .arg(&output)
        .current_dir(&temp.0)
        .output()
        .unwrap();
    assert!(!result.status.success());
    assert!(String::from_utf8_lossy(&result.stderr).contains("distinct"));
    assert!(!output.exists());
    for filename in ["FUTURE.xml", ".xml"] {
        let new_xml = source.join("monitor").join(filename);
        let result = Command::new(BIN)
            .arg("convert")
            .arg(&source)
            .arg(&new_xml)
            .output()
            .unwrap();
        assert!(!result.status.success());
        assert!(!new_xml.exists());
        assert!(String::from_utf8_lossy(&result.stderr).contains("source XML"));
    }
}

#[test]
fn comments_and_cdata_can_mention_document_declarations() {
    parse_xml(b"<!-- <!DOCTYPE monitor> --><monitor><future><![CDATA[<!ENTITY example>]]></future></monitor>","monitor").unwrap();
}

#[cfg(unix)]
#[test]
fn fresh_output_aliases_through_symlinked_parent_are_rejected() {
    let temp = Temp::new();
    std::os::unix::fs::symlink(&temp.0, temp.0.join("alias")).unwrap();
    let output = Command::new(BIN)
        .arg("convert")
        .arg(fixtures().join("source"))
        .arg(temp.0.join("out.cbor"))
        .arg("--snapshot")
        .arg(temp.0.join("alias/out.cbor"))
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("distinct"));
    assert!(!temp.0.join("out.cbor").exists());
}

#[test]
fn all_source_attributes_elements_and_order_survive_conversion() {
    fn compare(element: roxmltree::Node<'_, '_>, encoded: &Value) {
        let kind = uint(field(encoded, 0).unwrap()).unwrap();
        let mut actual = std::collections::BTreeMap::<String, Value>::new();
        for (key, value) in map(field(encoded, 1).unwrap()).unwrap() {
            let id = uint(key).unwrap();
            let name = format::ATTRIBUTES.iter().find(|(n, _)| *n == id).unwrap().1;
            actual.insert(name.into(), value.clone());
        }
        let mut source_tag = format::kind_name(kind);
        if let Some(extensions) = optional(encoded, 3) {
            for extension in array(extensions).unwrap() {
                assert_eq!(field(extension, 0).unwrap().as_text(), Some(METADATA));
                let payload = field(extension, 2).unwrap();
                for (key, value) in map(field(payload, 0).unwrap()).unwrap() {
                    assert!(actual
                        .insert(key.as_text().unwrap().into(), value.clone())
                        .is_none());
                }
                if kind == 255 {
                    source_tag = optional(payload, 1).and_then(Value::as_text);
                }
            }
        }
        assert_eq!(source_tag, Some(element.tag_name().name()));
        assert_eq!(actual.len(), element.attributes().len());
        for attr in element.attributes() {
            let name = attr
                .namespace()
                .map(|ns| format!("{{{ns}}}{}", attr.name()))
                .unwrap_or_else(|| attr.name().into());
            let value = &actual[&name];
            if let Some(integer) = value.as_integer() {
                let expected = if name == "delay" {
                    attr.value()
                        .trim_start_matches(|c: char| c.is_ascii_whitespace())
                        .parse::<i64>()
                        .unwrap()
                } else {
                    ddccontrol_xml::parse_integer(attr.value()).unwrap()
                };
                assert_eq!(i128::from(integer), i128::from(expected));
            } else {
                assert_eq!(value.as_text(), Some(attr.value()));
            }
        }
        let children: Vec<_> = element
            .children()
            .filter(roxmltree::Node::is_element)
            .collect();
        let encoded_children = array(field(encoded, 2).unwrap()).unwrap();
        assert_eq!(children.len(), encoded_children.len());
        for (element, node) in children.into_iter().zip(encoded_children) {
            compare(element, node);
        }
    }
    let mut directories = vec![fixtures().join("source")];
    if let Some(directory) = std::env::var_os("DDCCONTROL_DB_TEST_DATADIR") {
        directories.push(directory.into());
    }
    for directory in directories {
        let root = convert(&directory, None).unwrap();
        let mut sources = vec![(directory.join("options.xml"), field(&root, 5).unwrap())];
        for (id, node) in map(field(&root, 6).unwrap()).unwrap() {
            sources.push((
                directory.join(format!("monitor/{}.xml", id.as_text().unwrap())),
                node,
            ));
        }
        assert_eq!(
            sources.len() - 1,
            fs::read_dir(directory.join("monitor"))
                .unwrap()
                .filter_map(Result::ok)
                .filter(|e| e.path().extension().is_some_and(|ext| ext == "xml"))
                .count()
        );
        for (path, node) in sources {
            let xml = format::xml::decode_source(&fs::read(path).unwrap()).unwrap();
            let parsed = roxmltree::Document::parse(&xml).unwrap();
            compare(parsed.root_element(), node);
        }
    }
}

#[cfg(unix)]
#[test]
fn output_cannot_replace_sources_linked_outside_the_database() {
    use std::os::unix::fs::symlink;
    for linked_monitor_directory in [false, true] {
        let temp = Temp::new();
        let source = temp.source();
        let (linked, external) = if linked_monitor_directory {
            let external = temp.0.join("external-monitor");
            fs::rename(source.join("monitor"), &external).unwrap();
            symlink(&external, source.join("monitor")).unwrap();
            (source.join("monitor/VESA.xml"), external.join("VESA.xml"))
        } else {
            let external = temp.0.join("external-options.xml");
            fs::rename(source.join("options.xml"), &external).unwrap();
            symlink(&external, source.join("options.xml")).unwrap();
            (source.join("options.xml"), external)
        };
        let original = fs::read(&external).unwrap();
        for destination in [&linked, &external] {
            for as_snapshot in [false, true] {
                let mut command = Command::new(BIN);
                command.arg("convert").arg(&source);
                if as_snapshot {
                    command
                        .arg(temp.0.join("out.cbor"))
                        .arg("--snapshot")
                        .arg(destination);
                } else {
                    command.arg(destination);
                }
                let result = command.output().unwrap();
                assert!(!result.status.success());
                assert!(String::from_utf8_lossy(&result.stderr).contains("source XML"));
                assert_eq!(fs::read(&external).unwrap(), original);
                assert_eq!(fs::read(&linked).unwrap(), original);
                assert!(!temp.0.join("out.cbor").exists());
            }
        }
        if linked_monitor_directory {
            assert!(fs::symlink_metadata(source.join("monitor"))
                .unwrap()
                .file_type()
                .is_symlink());
            let new_xml = source.join("monitor/NEW.xml");
            let result = Command::new(BIN)
                .arg("convert")
                .arg(&source)
                .arg(&new_xml)
                .output()
                .unwrap();
            assert!(!result.status.success());
            assert!(!new_xml.exists());
        } else {
            assert!(fs::symlink_metadata(&linked)
                .unwrap()
                .file_type()
                .is_symlink());
        }
    }
}
