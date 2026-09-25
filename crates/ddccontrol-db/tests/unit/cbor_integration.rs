// Included by monitor_db_compatibility.rs to share C ABI snapshot helpers.

struct TemporaryCborDatabase(PathBuf);

impl TemporaryCborDatabase {
    fn new() -> Self {
        static NEXT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
        let id = NEXT.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        let path = env::temp_dir().join(format!("ddccontrol-db-test-{}-{id}", std::process::id()));
        fs::create_dir(&path).unwrap();
        Self(path)
    }

    fn xml_fixture() -> Self {
        let output = Self::new();
        fs::create_dir(output.0.join("monitor")).unwrap();
        // distcheck makes distributed sources read-only. Copy bytes rather
        // than source permissions because these tests edit the temporary view.
        fs::write(
            output.0.join("options.xml"),
            fs::read(fixture_datadir().join("options.xml")).unwrap(),
        )
        .unwrap();
        for profile in ["compat-common", "compat-monitor"] {
            fs::write(
                output.0.join(format!("monitor/{profile}.xml")),
                fs::read(fixture_datadir().join(format!("monitor/{profile}.xml"))).unwrap(),
            )
            .unwrap();
        }
        output
    }

    fn install_cbor(&self) {
        fs::write(
            self.0.join("ddccontrol-db.cbor"),
            include_bytes!("../../fixtures/cbor/compat.cbor"),
        )
        .unwrap();
    }
}

impl Drop for TemporaryCborDatabase {
    fn drop(&mut self) {
        fs::remove_dir_all(&self.0).unwrap();
    }
}

fn reinitialize(directory: &Path) -> c_int {
    let path = path_to_cstring(directory);
    unsafe { ddcci_init_db(path.as_ptr() as *mut c_char) }
}

fn compare_profile_scenarios(
    profiles: &[String],
) -> Vec<(Option<String>, ddccontrol_caps::Caps, c_int)> {
    let mut snapshots = Vec::new();
    for profile in profiles {
        for tolerant in [false, true] {
            for scenario in 0..3 {
                let mut caps = match scenario {
                    0 => OwnedCaps::with_all_vcp_codes(),
                    1 => OwnedCaps::empty(),
                    _ => OwnedCaps::from_str("(prot(monitor)type(LCD)vcp(04 10 60(11) C8(1234)))"),
                };
                let monitor = OwnedMonitor::create(profile, &mut caps, tolerant);
                snapshots.push((
                    monitor.as_ref().map(OwnedMonitor::snapshot),
                    unsafe { crate::caps_from_c(&mut caps.0) },
                    ddcci_db_requirements_failed(),
                ));
            }
        }
    }
    snapshots
}

#[test]
fn frozen_cbor_and_xml_build_identical_trees_caps_and_failures() {
    let temporary = TemporaryCborDatabase::new();
    temporary.install_cbor();
    let _context = DbTestContext::init(&fixture_datadir());
    let profiles = vec![
        "compat-monitor".to_string(),
        "compat-common".to_string(),
        "missing".to_string(),
    ];
    let xml = compare_profile_scenarios(&profiles);
    assert_eq!(reinitialize(&temporary.0), 1);
    assert_eq!(compare_profile_scenarios(&profiles), xml);
}

#[test]
fn present_cbor_is_preferred_and_failed_cbor_cannot_fall_back_to_xml() {
    let temporary = TemporaryCborDatabase::xml_fixture();
    temporary.install_cbor();
    // XML belongs to an older/different snapshot: it must never be mixed in.
    fs::write(temporary.0.join("options.xml"), "not XML").unwrap();
    let _context = DbTestContext::init(&temporary.0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_some());
    fs::write(temporary.0.join("ddccontrol-db.cbor"), [0xff]).unwrap();
    assert_eq!(reinitialize(&temporary.0), 0);
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
}

#[test]
fn explicit_xml_directory_and_profile_snapshots_survive_file_replacement() {
    let temporary = TemporaryCborDatabase::xml_fixture();
    let other = TemporaryCborDatabase::new();
    other.install_cbor();
    let _context = DbTestContext::init(&other.0);
    let profile = temporary.0.join("monitor/compat-monitor.xml");
    let source = fs::read_to_string(&profile).unwrap();
    fs::write(
        &profile,
        source.replace("Compatibility Fixture Monitor", "Selected XML directory"),
    )
    .unwrap();
    assert_eq!(reinitialize(&temporary.0), 1);
    fs::write(
        &profile,
        source.replace("Compatibility Fixture Monitor", "Replacement XML profile"),
    )
    .unwrap();
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let first = OwnedMonitor::create("compat-monitor", &mut caps, true).unwrap();
    assert!(first.snapshot().contains("Selected XML directory"));
    assert_eq!(reinitialize(&temporary.0), 1);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true)
        .unwrap()
        .snapshot()
        .contains("Replacement XML profile"));
    unsafe { ddcci_release_db() };
    assert!(first.snapshot().contains("Selected XML directory"));
}

#[test]
fn xml_snapshot_manifest_rejects_inconsistent_sources() {
    let temporary = TemporaryCborDatabase::xml_fixture();
    fs::write(
        temporary.0.join("ddccontrol-db.snapshot"),
        include_bytes!("../../fixtures/cbor/compat.snapshot"),
    )
    .unwrap();
    let _context = DbTestContext::init(&temporary.0);
    let mut bytes = fs::read(temporary.0.join("monitor/compat-common.xml")).unwrap();
    bytes.extend_from_slice(b"\n");
    fs::write(temporary.0.join("monitor/compat-common.xml"), bytes).unwrap();
    assert_eq!(reinitialize(&temporary.0), 0);
}

#[test]
fn malformed_unrelated_xml_profile_does_not_disable_known_profiles() {
    let temporary = TemporaryCborDatabase::xml_fixture();
    fs::write(temporary.0.join("monitor/bad.xml"), "not XML").unwrap();
    let _context = DbTestContext::init(&temporary.0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_some());
    assert!(OwnedMonitor::create("bad", &mut caps, true).is_none());
}

#[test]
fn whole_database_xml_cbor_semantics_match_when_configured() {
    let Some(datadir) = env::var_os("DDCCONTROL_DB_TEST_DATADIR") else {
        return;
    };
    let datadir = resolve_real_database_datadir(PathBuf::from(datadir));
    // This integration explicitly covers every profile even if the older smoke
    // test is configured to use only a small representative subset.
    let mut profiles: Vec<String> = fs::read_dir(datadir.join("monitor"))
        .unwrap()
        .map(Result::unwrap)
        .filter_map(|entry| {
            entry
                .file_name()
                .to_str()?
                .strip_suffix(".xml")
                .map(ToString::to_string)
        })
        .collect();
    profiles.sort();
    let converter = env::var_os("DDCCONTROL_DB_CONVERTER")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("ddccontrol-dbgen"));
    let converted = TemporaryCborDatabase::new();
    let output = std::process::Command::new(&converter)
        .arg("convert")
        .arg(&datadir)
        .arg(converted.0.join("ddccontrol-db.cbor"))
        .output()
        .unwrap_or_else(|error| {
            panic!(
                "cannot run {}: {error}; build ddccontrol-dbgen and set DDCCONTROL_DB_CONVERTER to its executable",
                converter.display()
            )
        });
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    // Ignore any installed CBOR in the source checkout by copying exactly its
    // XML snapshot into a fresh explicit directory.
    let xml_directory = TemporaryCborDatabase::new();
    fs::create_dir(xml_directory.0.join("monitor")).unwrap();
    for (path, bytes) in read_xml_snapshot(&datadir).unwrap() {
        fs::write(xml_directory.0.join(path), bytes).unwrap();
    }
    let _context = DbTestContext::init(&xml_directory.0);
    let xml = compare_profile_scenarios(&profiles);
    assert_eq!(reinitialize(&converted.0), 1);
    let cbor = compare_profile_scenarios(&profiles);
    for (index, (expected, actual)) in xml.iter().zip(&cbor).enumerate() {
        assert_eq!(
            actual,
            expected,
            "profile {} scenario {}",
            profiles[index / 6],
            index % 6
        );
    }
    eprintln!(
        "Compared {} profiles × 3 CAPS inputs × strict/tolerant modes",
        profiles.len()
    );
}

fn wire_field(value: &mut ciborium::value::Value, key: u64) -> &mut ciborium::value::Value {
    value
        .as_map_mut()
        .unwrap()
        .iter_mut()
        .find_map(|(candidate, value)| {
            (candidate
                .as_integer()
                .and_then(|number| u64::try_from(number).ok())
                == Some(key))
            .then_some(value)
        })
        .unwrap()
}

fn wire_profile<'a>(
    root: &'a mut ciborium::value::Value,
    name: &str,
) -> &'a mut ciborium::value::Value {
    wire_field(root, 6)
        .as_map_mut()
        .unwrap()
        .iter_mut()
        .find_map(|(key, value)| (key.as_text() == Some(name)).then_some(value))
        .unwrap()
}

fn attach_test_requirement(node: &mut ciborium::value::Value, field_id: u64, required: bool) {
    use ciborium::value::Value;
    let extension = Value::Map(vec![
        (
            Value::Integer(0.into()),
            Value::Text("https://example.test/ddccontrol/future-matcher/1".into()),
        ),
        (Value::Integer(1.into()), Value::Bool(required)),
        (Value::Integer(2.into()), Value::Bytes(vec![0, 1, 255])),
    ]);
    let fields = node.as_map_mut().unwrap();
    if let Some((_, value)) = fields.iter_mut().find(|(key, _)| {
        key.as_integer()
            .and_then(|number| u64::try_from(number).ok())
            == Some(field_id)
    }) {
        value.as_array_mut().unwrap().push(extension);
    } else {
        fields.push((
            Value::Integer(field_id.into()),
            Value::Array(vec![extension]),
        ));
        fields.sort_by_key(|(key, _)| {
            let mut encoded = Vec::new();
            ciborium::into_writer(key, &mut encoded).unwrap();
            encoded
        });
    }
}

fn modified_wire_fixture(change: impl FnOnce(&mut ciborium::value::Value)) -> TemporaryCborDatabase {
    let temporary = TemporaryCborDatabase::new();
    let mut root: ciborium::value::Value =
        ciborium::from_reader(&include_bytes!("../../fixtures/cbor/compat.cbor")[..]).unwrap();
    change(&mut root);
    let mut bytes = Vec::new();
    ciborium::into_writer(&root, &mut bytes).unwrap();
    fs::write(temporary.0.join("ddccontrol-db.cbor"), bytes).unwrap();
    temporary
}

#[test]
fn wire_unknown_optional_features_preserve_semantics_but_required_profile_rejects() {
    let optional = modified_wire_fixture(|root| {
        attach_test_requirement(wire_profile(root, "compat-monitor"), 3, false);
    });
    let required = modified_wire_fixture(|root| {
        attach_test_requirement(wire_profile(root, "compat-monitor"), 3, true);
    });
    let _context = DbTestContext::init(&optional.0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, false).is_some());
    assert_eq!(reinitialize(&required.0), 1);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
    assert_eq!(ddcci_db_requirements_failed(), 1);
    assert!(OwnedMonitor::create("compat-common", &mut caps, true).is_some());
}

#[test]
fn wire_required_control_and_value_features_disable_only_their_control() {
    for value_scope in [false, true] {
        let temporary = modified_wire_fixture(|root| {
            let profile = wire_profile(root, "compat-monitor");
            // Frozen profile children are caps, caps, include, controls.
            let controls = &mut wire_field(profile, 2).as_array_mut().unwrap()[3];
            let control = &mut wire_field(controls, 2).as_array_mut().unwrap()[0];
            let target = if value_scope {
                &mut wire_field(control, 2).as_array_mut().unwrap()[0]
            } else {
                control
            };
            attach_test_requirement(target, 3, true);
        });
        let _context = DbTestContext::init(&temporary.0);
        let mut caps = OwnedCaps::with_all_vcp_codes();
        let monitor = OwnedMonitor::create("compat-monitor", &mut caps, false).unwrap();
        assert!(!monitor.snapshot().contains("control id=color_preset"));
        assert!(monitor.snapshot().contains("control id=brightness"));
        assert_eq!(ddcci_db_requirements_failed(), 0);
    }
}

#[test]
fn wire_required_database_features_reject_the_database() {
    let temporary = modified_wire_fixture(|root| attach_test_requirement(root, 7, true));
    let _context = DbTestContext::init(&fixture_datadir());
    assert_eq!(reinitialize(&temporary.0), 0);
}

#[test]
fn wire_required_include_or_caps_effect_rejects_the_profile() {
    for index in [0, 2] {
        let temporary = modified_wire_fixture(|root| {
            let operation = &mut wire_field(wire_profile(root, "compat-monitor"), 2)
                .as_array_mut()
                .unwrap()[index];
            attach_test_requirement(operation, 3, true);
        });
        let _context = DbTestContext::init(&temporary.0);
        let mut caps = OwnedCaps::with_all_vcp_codes();
        let original = unsafe { crate::caps_from_c(&mut caps.0) };
        assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
        assert_eq!(ddcci_db_requirements_failed(), 1);
        assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
        assert!(OwnedMonitor::create("compat-common", &mut caps, true).is_some());
    }
}

#[test]
fn ordinary_profile_errors_cannot_bypass_required_semantics() {
    for scope in ["operation", "include", "control", "shared-control"] {
        for required in [false, true] {
            for error_first in [false, true] {
                let temporary = modified_wire_fixture(|root| {
                    use ciborium::value::Value;
                    let profile = wire_profile(root, "compat-monitor");
                    if error_first {
                        // Fail before visiting the required feature.
                        *wire_field(wire_field(profile, 1), 5) = Value::Text("invalid-init".into());
                    } else {
                        // A second controls block fails after the first block
                        // has already isolated any required control semantics.
                        wire_field(profile, 2)
                            .as_array_mut()
                            .unwrap()
                            .push(Value::Map(vec![
                                (Value::Integer(0.into()), Value::Integer(8.into())),
                                (Value::Integer(1.into()), Value::Map(vec![])),
                                (Value::Integer(2.into()), Value::Array(vec![])),
                            ]));
                    }
                    let target = match scope {
                        "operation" => &mut wire_field(wire_profile(root, "compat-monitor"), 2)
                            .as_array_mut()
                            .unwrap()[0],
                        "include" => wire_profile(root, "compat-common"),
                        "control" => {
                            let controls = &mut wire_field(wire_profile(root, "compat-monitor"), 2)
                                .as_array_mut()
                                .unwrap()[3];
                            &mut wire_field(controls, 2).as_array_mut().unwrap()[0]
                        }
                        "shared-control" => {
                            let group =
                                &mut wire_field(wire_field(root, 5), 2).as_array_mut().unwrap()[0];
                            let subgroup = &mut wire_field(group, 2).as_array_mut().unwrap()[0];
                            &mut wire_field(subgroup, 2).as_array_mut().unwrap()[0]
                        }
                        _ => unreachable!(),
                    };
                    attach_test_requirement(target, 3, required);
                });
                let _context = DbTestContext::init(&temporary.0);
                for tolerant in [false, true] {
                    let mut caps = OwnedCaps::with_all_vcp_codes();
                    let original = unsafe { crate::caps_from_c(&mut caps.0) };
                    assert!(OwnedMonitor::create("compat-monitor", &mut caps, tolerant).is_none());
                    assert_eq!(
                        ddcci_db_requirements_failed(),
                        c_int::from(required),
                        "scope: {scope}, required: {required}, error_first: {error_first}"
                    );
                    assert_eq!(unsafe { crate::caps_from_c(&mut caps.0) }, original);
                    // A following ordinary missing-profile call must reset the
                    // thread-local result, so normal generic fallback works.
                    assert!(OwnedMonitor::create("missing", &mut caps, tolerant).is_none());
                    assert_eq!(ddcci_db_requirements_failed(), 0);
                }
            }
        }
    }
}

#[test]
fn unknown_required_control_id_cannot_be_reintroduced_by_generic_include() {
    let temporary = modified_wire_fixture(|root| {
        use ciborium::value::Value;
        let profile = wire_profile(root, "compat-monitor");
        let controls = &mut wire_field(profile, 2).as_array_mut().unwrap()[3];
        let mut control = Value::Map(vec![
            (Value::Integer(0.into()), Value::Integer(3.into())),
            (
                Value::Integer(1.into()),
                Value::Map(vec![
                    (
                        Value::Integer(1.into()),
                        Value::Text("unknown-future-id".into()),
                    ),
                    (Value::Integer(6.into()), Value::Integer(0x10.into())),
                ]),
            ),
            (Value::Integer(2.into()), Value::Array(vec![])),
        ]);
        attach_test_requirement(&mut control, 3, true);
        wire_field(controls, 2)
            .as_array_mut()
            .unwrap()
            .push(control);
    });
    let _context = DbTestContext::init(&temporary.0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    // In the frozen fixture, the included known brightness was already added.
    // Required semantics still remove that address instead of leaving it live.
    let monitor = OwnedMonitor::create("compat-monitor", &mut caps, false).unwrap();
    assert!(!monitor.snapshot().contains("control id=brightness"));
    assert!(monitor.snapshot().contains("control id=input"));
    assert_eq!(ddcci_db_requirements_failed(), 0);
}

#[test]
fn required_shared_control_with_new_type_is_isolated_without_interpreting_it() {
    let temporary = modified_wire_fixture(|root| {
        use ciborium::value::Value;
        let options = wire_field(root, 5);
        let group = &mut wire_field(options, 2).as_array_mut().unwrap()[0];
        let subgroup = &mut wire_field(group, 2).as_array_mut().unwrap()[0];
        let control = &mut wire_field(subgroup, 2).as_array_mut().unwrap()[0];
        *wire_field(wire_field(control, 1), 2) = Value::Text("future-block-operation".into());
        attach_test_requirement(control, 3, true);
    });
    let _context = DbTestContext::init(&temporary.0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    let monitor = OwnedMonitor::create("compat-monitor", &mut caps, false).unwrap();
    assert!(!monitor.snapshot().contains("control id=brightness"));
    assert!(monitor.snapshot().contains("control id=input"));
}

#[test]
fn missing_cbor_cannot_bypass_a_guarded_xml_snapshot() {
    let temporary = TemporaryCborDatabase::xml_fixture();
    // Future producers must replace the manifest authorization with this
    // permanent guard if legacy XML cannot express required CBOR semantics.
    fs::write(temporary.0.join("ddccontrol-db.snapshot"), [0xf4]).unwrap();
    let _context = DbTestContext::init(&fixture_datadir());
    assert_eq!(reinitialize(&temporary.0), 0);
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_none());
    // The guard governs XML fallback only; ordinary validated CBOR still loads.
    temporary.install_cbor();
    assert_eq!(reinitialize(&temporary.0), 1);
    assert!(OwnedMonitor::create("compat-monitor", &mut caps, true).is_some());
}

#[test]
fn local_xml_override_uses_pinned_cbor_includes_and_preserves_required_rejection() {
    let database = TemporaryCborDatabase::xml_fixture();
    let path = database.0.join("DEL1234.xml");
    fs::write(&path, r#"<monitor name="Local preview" init="standard">
        <caps add="(vcp(60 C8))" remove="(vcp(10))"/>
        <include file="compat-common"/></monitor>"#).unwrap();
    let _context = DbTestContext::init(&database.0);
    assert_eq!(set_monitor_file(&path), 1);
    let expected = compare_profile_scenarios(&["DEL1234".into()]);
    database.install_cbor();
    // Neither malformed source XML nor replaced CBOR can alter this session.
    fs::write(database.0.join("monitor/compat-common.xml"), "invalid XML").unwrap();
    assert_eq!(reinitialize(&database.0), 1);
    assert_eq!(set_monitor_file(&path), 1);
    fs::write(database.0.join("ddccontrol-db.cbor"), [0xff]).unwrap();
    assert_eq!(compare_profile_scenarios(&["DEL1234".into()]), expected);

    let required = modified_wire_fixture(|root| {
        attach_test_requirement(wire_profile(root, "compat-common"), 3, true);
    });
    assert_eq!(reinitialize(&required.0), 1);
    // A failed include validation must not replace an already valid override.
    let good = required.0.join("ACR1234.xml");
    fs::write(&good, LOCAL_MONITOR).unwrap();
    assert_eq!(set_monitor_file(&good), 1);
    assert_eq!(set_monitor_file(&path), 0);
    assert!(monitor_file_matches("ACR1234"));
    assert!(!monitor_file_matches("DEL1234"));
    let mut caps = OwnedCaps::with_all_vcp_codes();
    assert!(OwnedMonitor::create("ACR1234", &mut caps, true).is_some());
    assert!(OwnedMonitor::create("compat-common", &mut caps, true).is_none());
    assert_eq!(ddcci_db_requirements_failed(), 1);
}
