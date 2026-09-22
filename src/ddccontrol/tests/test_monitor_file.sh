#!/bin/sh
# Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
set -eu
: "${abs_top_builddir:?provided by make check}"
: "${abs_top_srcdir:?provided by make check}"

test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
cli="$abs_top_builddir/src/ddccontrol/ddccontrol"
db="$test_root/db"
mkdir -p "$db/monitor" "$test_root/valid" "$test_root/invalid"
cp "$abs_top_srcdir/crates/ddccontrol-db/fixtures/compat-db/options.xml" "$db/"
cp "$abs_top_srcdir/crates/ddccontrol-db/fixtures/compat-db/monitor/compat-common.xml" "$db/monitor/"

# A deliberately invalid installed entry proves the external file is used.
echo 'do not load or change this installed definition' > "$db/monitor/DEL1234.xml"
cp "$db/monitor/DEL1234.xml" "$test_root/original.xml"
valid="$test_root/valid/DEL1234.xml"
cat > "$valid" <<'XML'
<monitor name="Local XML override test" init="standard">
  <caps add="(vcp(10))"/>
  <controls><control id="brightness" address="0x10"/></controls>
</monitor>
XML
cat > "$test_root/invalid/DEL1234.xml" <<'XML'
<monitor name="Invalid override" init="standard">
  <caps add="(vcp(10))"/>
  <controls><control id="not_an_option" address="0x10"/></controls>
</monitor>
XML

# Integrity/argument checks must never access a real bus or hardware.
export LC_ALL=C
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$test_root/no-system-bus"
expect_failure() {
    if "$@" > "$test_root/output" 2>&1; then
        echo "Expected failure: $*" >&2
        exit 1
    fi
    if grep -E 'Failed to open D-Bus proxy|cannot open display' "$test_root/output"; then
        echo 'Validation happened too late' >&2
        exit 1
    fi
}

"$cli" -b "$db" --monitor-file "$valid" -i DEL1234 > "$test_root/output" 2>&1
expect_failure "$cli" -b "$db" -i DEL1234
expect_failure "$cli" -b "$db" --monitor-file "$valid" -i ACR1234
expect_failure "$cli" -b "$db" --monitor-file "$valid" -p
expect_failure "$cli" -b "$db" --monitor-file
expect_failure "$cli" -b "$db" --monitor-file "$valid" --monitor-file "$valid"
expect_failure "$cli" -b "$db" --monitor-file "$test_root/invalid/DEL1234.xml"
expect_failure env DDCCONTROL_NO_DAEMON=1 "$cli" -b "$db" --monitor-file "$test_root/missing/DEL1234.xml"
cp "$valid" "$test_root/wrong-name.xml"
expect_failure "$cli" -b "$db" --monitor-file "$test_root/wrong-name.xml"

# Includes still resolve from the chosen database, without installing the root XML.
cat > "$valid" <<'XML'
<monitor name="Local XML override test" init="standard">
  <caps add="(vcp(10))"/>
  <include file="compat-common"/>
</monitor>
XML
"$cli" -b "$db" --monitor-file "$valid" -i DEL1234 > "$test_root/output" 2>&1

# Exercise real C frontend + DB loader + D-Bus client against the existing private
# test service when the optional integration-test dependencies are available.
if command -v dbus-run-session >/dev/null 2>&1 &&
   /usr/bin/python3 -c 'from gi.repository import Gio, GLib' >/dev/null 2>&1; then
    mock="$abs_top_srcdir/crates/ddccontrol-scanmonitor/tests/dbus_service.py"
    env -u DDCCONTROL_NO_DAEMON dbus-run-session -- /usr/bin/python3 "$mock" normal \
        "$cli" -b "$db" --monitor-file "$valid" > "$test_root/output" 2>&1
    grep 'Local XML override test' "$test_root/output"
    grep 'supported, value=42, maximum=100' "$test_root/output"
    # The fixture rejects SetControl. A mismatching file must fail before writing.
    cp "$valid" "$test_root/valid/ACR1234.xml"
    if env -u DDCCONTROL_NO_DAEMON dbus-run-session -- /usr/bin/python3 "$mock" normal \
        "$cli" -b "$db" --monitor-file "$test_root/valid/ACR1234.xml" \
        -r 0x10 -w 50 dev:/dev/i2c-4 > "$test_root/output" 2>&1; then
        echo 'Mismatching monitor file accepted' >&2
        exit 1
    fi
    grep 'selected monitor is DEL1234' "$test_root/output"
    if grep 'Unexpected calls' "$test_root/output"; then exit 1; fi
else
    echo 'Private D-Bus frontend checks skipped: install python3-gi and dbus-daemon.'
fi
cmp "$test_root/original.xml" "$db/monitor/DEL1234.xml"
