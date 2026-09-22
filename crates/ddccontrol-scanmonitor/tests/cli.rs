// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

use std::process::Command;

fn command() -> Command {
    let mut command = Command::new(env!("CARGO_BIN_EXE_ddccontrol-scanmonitor"));
    // Tests must never contact a real system daemon or a monitor.
    command.env(
        "DBUS_SYSTEM_BUS_ADDRESS",
        "unix:path=/nonexistent/ddccontrol-test-bus",
    );
    command
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
fn unavailable_bus_returns_actionable_error() {
    let output = command().arg("--list").output().unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8(output.stderr)
        .unwrap()
        .contains("ddccontrol"));
    assert!(output.stdout.is_empty());
}
