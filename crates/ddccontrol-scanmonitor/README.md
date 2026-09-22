# ddccontrol-scanmonitor

Create an editable monitor definition for `ddccontrol-db`:

```sh
ddccontrol-scanmonitor
```

One connected DDC/CI monitor is selected automatically. With several monitors,
choose a number, or pass a device explicitly:

```sh
ddccontrol-scanmonitor --list
ddccontrol-scanmonitor dev:/dev/i2c-4
```

The tool writes `<PNPID>.xml` in the current directory, using the monitor's EDID
identifier and name. It never overwrites an existing file; use `--output FILE`
to choose another path. `--db-path DIR` selects a database containing
`options.xml`. The system D-Bus service supplied with ddccontrol handles device
access, so the scanner does not need to run as root.

Known, unambiguous controls with plausible readings are enabled. Ambiguous
addresses, commands, vendor controls and unconfirmed enum mappings remain
commented for investigation. The scan reads controls and does not send
`SetControl` requests; the daemon performs its usual monitor initialization.
Successful reads do not prove that writing a control will work correctly.

Review the XML comments and monitor name, then test it directly:

```sh
gddccontrol --monitor-file ./DEL1234.xml
ddccontrol --monitor-file ./DEL1234.xml
```

Use the actual `<PNPID>.xml` filename; it determines which monitor gets the
definition. Relaunch the application after edits. Set `DDCCONTROL_NO_DAEMON=1`
to use direct device access with sufficient `/dev/i2c-*` permissions. This also
tests the file's monitor initialization and write delays; in daemon mode, those
still use the service's installed definition.

Enable additional entries as you verify them. For permanent use, copy the file
into the installed database's `monitor/` directory (the scanner prints the
destination), then restart `ddccontrol.service`.
Some Samsung monitors require `init="samsung"`; the generated definition uses
`init="standard"` and should be adjusted if the monitor needs that initialization.
Submit the tested XML under `db/monitor/` in a pull request to
[ddccontrol-db](https://github.com/ddccontrol/ddccontrol-db).

## Development

The normal Autotools build installs this binary. It can also be built directly:

```sh
cargo build --release -p ddccontrol-scanmonitor --locked
cargo test -p ddccontrol-scanmonitor --locked
```

Rust 1.77+ and GLib/GIO development libraries are required (`libglib2.0-dev` on
Debian/Ubuntu). The crate reuses the workspace's EDID and capabilities parsers
and XML dependency; it introduces no new Cargo dependencies. GIO uses the
existing `ddccontrol.DDCControl` system D-Bus API. Its C ABI declarations use
opaque pointers and C-compatible integer types without architecture-specific
layouts or native byte-order assumptions.

The end-to-end tests use a simulated service on a private D-Bus, without
accessing connected monitors. Install `python3-gi` and `dbus-daemon` to run them;
they are skipped when those test dependencies are absent. CI installs both.

Direct Cargo builds look for the database in `/usr/share/ddccontrol-db`, then
`/usr/local/share/ddccontrol-db`. Autotools builds use the configured data
directory. `--db-path` overrides either default.
