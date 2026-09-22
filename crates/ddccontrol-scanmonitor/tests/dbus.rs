// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use std::fs;
use std::path::PathBuf;
use std::process::{Command, Output, Stdio};
use std::sync::atomic::{AtomicUsize, Ordering};

const OPTIONS: &str = r#"<options dbversion="3" date="2026-09-22">
  <group name="Picture"><subgroup name="Basic">
    <control id="brightness" name="Brightness" address="0x10" type="value"/>
    <control id="contrast" name="Contrast" address="0x12" type="value"/>
    <control id="red" name="Red" address="0x16" type="value"/>
  </subgroup></group>
</options>"#;

struct Fixture(PathBuf);

impl Fixture {
    fn new() -> Option<Self> {
        let python_available = Command::new("/usr/bin/python3")
            .args(["-c", "from gi.repository import Gio, GLib"])
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .is_ok_and(|status| status.success());
        let dbus_available = Command::new("dbus-run-session")
            .arg("--version")
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .is_ok_and(|status| status.success());
        if !python_available || !dbus_available {
            eprintln!("Skipping private D-Bus test: install python3-gi and dbus-daemon");
            return None;
        }
        static NEXT: AtomicUsize = AtomicUsize::new(0);
        let path = std::env::temp_dir().join(format!(
            "ddccontrol-scanmonitor-dbus-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        fs::write(path.join("options.xml"), OPTIONS).unwrap();
        Some(Self(path))
    }

    fn run(&self, scenario: &str, arguments: &[&str]) -> Output {
        Command::new("dbus-run-session")
            .arg("--")
            .arg("/usr/bin/python3")
            .arg(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/tests/dbus_service.py"
            ))
            .arg(scenario)
            .arg(env!("CARGO_BIN_EXE_ddccontrol-scanmonitor"))
            .args(["--db-path", "."])
            .args(arguments)
            .current_dir(&self.0)
            .stdin(Stdio::null())
            .output()
            .unwrap()
    }

    fn xml(&self, filename: &str) -> String {
        fs::read_to_string(self.0.join(filename)).unwrap()
    }
}

impl Drop for Fixture {
    fn drop(&mut self) {
        fs::remove_dir_all(&self.0).unwrap();
    }
}

fn success(output: &Output) {
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
}

#[test]
fn discovers_scans_and_writes_a_valid_profile_without_setting_controls() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    let output = fixture.run("normal", &[]);
    success(&output);
    let xml = fixture.xml("DEL1234.xml");
    let document = roxmltree::Document::parse(&xml).unwrap();
    assert_eq!(
        document.root_element().attribute("name"),
        Some("Test & Screen")
    );
    let controls: Vec<_> = document
        .descendants()
        .filter(|node| node.has_tag_name("control"))
        .map(|node| node.attribute("id").unwrap())
        .collect();
    assert_eq!(controls, ["brightness"]);
    assert!(xml.contains("current=42, maximum=100"));
    assert!(xml.contains("Candidate contrast"));
    assert!(xml.contains("Candidate red"));
    assert!(String::from_utf8_lossy(&output.stdout).contains("DEL1234.xml"));
}

#[test]
fn list_filters_unsupported_displays_and_does_not_write_a_profile() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    let output = fixture.run("normal", &["--list"]);
    success(&output);
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("Fixture Monitor"));
    assert!(!stdout.contains("Unsupported Panel"));
    assert!(!fixture.0.join("DEL1234.xml").exists());
}

#[test]
fn multiple_displays_require_selection_and_explicit_device_works() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    let output = fixture.run("multiple", &[]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("more than one monitor"));
    assert!(!fixture.0.join("DEL1234.xml").exists());
    success(&fixture.run("multiple", &["dev:/dev/i2c-4", "--output", "chosen.xml"]));
    assert!(fixture.0.join("chosen.xml").exists());
}

#[test]
fn absent_monitors_invalid_identification_and_failed_reads_leave_no_profile() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    for scenario in ["none", "invalid_pnp", "unreadable", "bad_lists"] {
        let output = fixture.run(scenario, &[]);
        assert!(!output.status.success(), "{scenario}");
        assert_ne!(
            output.status.code(),
            Some(99),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert_eq!(fs::read_dir(&fixture.0).unwrap().count(), 1, "{scenario}");
    }
}

#[test]
fn older_daemons_and_broken_capabilities_still_produce_usable_xml() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    for scenario in ["legacy", "no_edid", "bad_caps"] {
        let filename = format!("{scenario}.xml");
        let output = fixture.run(scenario, &["--output", &filename]);
        success(&output);
        let xml = fixture.xml(&filename);
        let document = roxmltree::Document::parse(&xml).unwrap();
        assert!(
            document
                .descendants()
                .any(|node| node.has_tag_name("control")
                    && node.attribute("id") == Some("brightness"))
        );
    }
}

#[test]
fn an_existing_output_is_never_overwritten() {
    let Some(fixture) = Fixture::new() else {
        return;
    };
    fs::write(fixture.0.join("existing.xml"), "keep my changes").unwrap();
    let output = fixture.run("normal", &["--output", "existing.xml"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("already exists"));
    assert_eq!(fixture.xml("existing.xml"), "keep my changes");
}
