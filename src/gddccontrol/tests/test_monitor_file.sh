#!/bin/sh
# Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
set -eu
: "${abs_top_builddir:?provided by make check}"
: "${abs_top_srcdir:?provided by make check}"

test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
gui="$abs_top_builddir/src/gddccontrol/gddccontrol"
db="$abs_top_srcdir/crates/ddccontrol-db/fixtures/compat-db"
export LC_ALL=C
unset DISPLAY WAYLAND_DISPLAY DDCCONTROL_NO_DAEMON
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$test_root/no-system-bus"

"$gui" --help > "$test_root/help" 2>&1
grep -- '--monitor-file' "$test_root/help"
grep -- '--db-path' "$test_root/help"

expect_failure() {
    if "$@" > "$test_root/output" 2>&1; then
        echo "Expected failure: $*" >&2
        exit 1
    fi
    if grep -E 'cannot open display|Failed to open D-Bus proxy' "$test_root/output"; then
        echo 'Validation happened after GTK or daemon access' >&2
        exit 1
    fi
    test -s "$test_root/output"
}

# Reject daemon mode before opening the database, monitor file, or display.
expect_failure "$gui" --db-path "$test_root/missing-db" --monitor-file "$test_root/missing/DEL1234.xml"
grep -F 'requires DDCCONTROL_NO_DAEMON=1' "$test_root/output"
for direct in '' 0 false true 2; do
    expect_failure env DDCCONTROL_NO_DAEMON="$direct" "$gui" --db-path "$test_root/missing-db" --monitor-file "$test_root/missing/DEL1234.xml"
    grep -F 'requires DDCCONTROL_NO_DAEMON=1' "$test_root/output"
done

cat > "$test_root/DEL1234.xml" <<'XML'
<monitor name="Invalid GUI override" init="standard">
  <caps add="(vcp(10))"/>
  <controls><control id="not_an_option" address="0x10"/></controls>
</monitor>
XML
export DDCCONTROL_NO_DAEMON=1
for definition in "$test_root/DEL1234.xml" "$test_root/missing/DEL1234.xml"; do
    expect_failure "$gui" --db-path "$db" --monitor-file "$definition"
done
