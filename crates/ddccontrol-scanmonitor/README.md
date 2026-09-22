# ddccontrol-scanmonitor

Create an editable monitor definition for `ddccontrol-db` from a connected
monitor. See the [monitor contribution guide](../../README.md#contributing-to-the-monitor-database)
for scanning, testing and submitting the XML, or `man ddccontrol-scanmonitor`
for all command-line options.

## Development

The normal Autotools build installs this binary. It can also be built directly:

```sh
cargo build --release -p ddccontrol-scanmonitor --locked
cargo test -p ddccontrol-scanmonitor --locked
```

Rust 1.77+ and GLib/GIO development libraries are required (`libglib2.0-dev` on
Debian/Ubuntu). The crate reuses the workspace's EDID and capabilities parsers
and XML dependency; it introduces no new third-party Cargo dependencies. GIO
uses the existing `ddccontrol.DDCControl` system D-Bus API. Its C ABI declarations use
opaque pointers and C-compatible integer types without architecture-specific
layouts or native byte-order assumptions.

The scanner reuses `ddccontrol-db::options` for database reading and validation,
`ddccontrol-edid` for identification, and the shared XML text helpers. Its XML
module only indexes the vocabulary and generates the editable monitor definition.

The end-to-end tests use a simulated service on a private D-Bus, without
accessing connected monitors. Install `python3-gi` and `dbus-daemon` to run them;
they are skipped when those test dependencies are absent. CI installs both.

Direct Cargo builds look for the database in `/usr/share/ddccontrol-db`, then
`/usr/local/share/ddccontrol-db`. Autotools builds use the configured data
directory. `--db-path` overrides either default.
