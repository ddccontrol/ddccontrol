# ddccontrol-scanmonitor

Create an editable monitor definition for `ddccontrol-db` from a connected
monitor. See the [monitor contribution guide](../../README.md#contributing-to-the-monitor-database)
for scanning, testing and submitting the XML, or `man ddccontrol-scanmonitor`
for all command-line options.

The scanner always uses `/dev/i2c-*` directly. It needs read and write permission
on the device; use `sudo` if necessary. No environment variable is needed.

## Development

The normal Autotools build installs this binary. It can also be built directly:

```sh
cargo build --release -p ddccontrol-scanmonitor --locked
cargo test -p ddccontrol-scanmonitor --locked
```

Rust 1.77+ is required. The scanner reuses the workspace's DDC/CI protocol,
EDID and capabilities parsers, `ddccontrol-db::options` for database reading and
validation, and the shared XML text helpers. Direct I2C access uses the existing
`libc` dependency; no new third-party Cargo dependencies are needed. Its XML
module indexes the vocabulary and generates the editable monitor definition.

Backend tests use simulated I2C exchanges. The CLI tests also run without
accessing connected monitors.

Direct Cargo builds look for the database in `/usr/share/ddccontrol-db`, then
`/usr/local/share/ddccontrol-db`. Autotools builds use the configured data
directory. `--db-path` overrides either default.
