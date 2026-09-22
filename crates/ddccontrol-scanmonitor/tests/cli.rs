// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use std::process::Command;

fn command() -> Command {
    Command::new(env!("CARGO_BIN_EXE_ddccontrol-scanmonitor"))
}

#[test]
fn help_and_version_work_without_daemon_or_database() {
    let output = command().arg("--help").output().unwrap();
    assert!(output.status.success());
    let help = String::from_utf8(output.stdout).unwrap();
    assert!(help.contains("Usage: ddccontrol-scanmonitor"));
    assert!(help.contains("--output"));
    let output = command().arg("--version").output().unwrap();
    assert!(output.status.success());
    assert!(String::from_utf8(output.stdout)
        .unwrap()
        .starts_with("ddccontrol-scanmonitor "));
}

#[test]
fn invalid_arguments_fail_before_connecting() {
    let output = command().arg("--output").output().unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8(output.stderr)
        .unwrap()
        .contains("requires a path"));
}

#[test]
fn missing_device_returns_actionable_error_without_a_daemon() {
    // An explicit nonexistent device prevents tests from probing real monitors.
    let device = "dev:/dev/i2c-4294967295";
    assert!(!std::path::Path::new("/dev/i2c-4294967295").exists());
    let output = command()
        .args([
            "--db-path",
            concat!(env!("CARGO_MANIFEST_DIR"), "/fixtures"),
        ])
        .arg(device)
        .env("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/test-bus")
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8(output.stderr).unwrap().contains(device));
    assert!(output.stdout.is_empty());
}
