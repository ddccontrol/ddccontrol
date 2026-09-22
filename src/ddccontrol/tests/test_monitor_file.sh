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
unset DDCCONTROL_NO_DAEMON
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

expect_direct_mode_required() {
    expect_failure "$@" "$cli" -b "$test_root/missing-db" --monitor-file "$test_root/missing/DEL1234.xml"
    grep -F 'requires DDCCONTROL_NO_DAEMON=1' "$test_root/output"
    expect_failure "$@" "$cli" -b "$db" --monitor-file "$valid" -i DEL1234
    grep -F 'requires DDCCONTROL_NO_DAEMON=1' "$test_root/output"
}

# Reject daemon mode before database/file access, including integrity checks.
expect_direct_mode_required env
for direct in '' 0 false true 2; do
    expect_direct_mode_required env DDCCONTROL_NO_DAEMON="$direct"
done

export DDCCONTROL_NO_DAEMON=1
"$cli" -b "$db" --monitor-file "$valid" -i DEL1234 > "$test_root/output" 2>&1
expect_failure "$cli" -b "$db" -i DEL1234
expect_failure "$cli" -b "$db" --monitor-file "$valid" -i ACR1234
expect_failure "$cli" -b "$db" --monitor-file "$valid" -p
expect_failure "$cli" -b "$db" --monitor-file
expect_failure "$cli" -b "$db" --monitor-file "$valid" --monitor-file "$valid"
expect_failure "$cli" -b "$db" --monitor-file "$test_root/invalid/DEL1234.xml"
expect_failure "$cli" -b "$db" --monitor-file "$test_root/missing/DEL1234.xml"
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

cmp "$test_root/original.xml" "$db/monitor/DEL1234.xml"
