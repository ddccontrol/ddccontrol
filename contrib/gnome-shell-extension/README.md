# ddccontrol GNOME Shell extension (example)

This example extension adds a monitor brightness item to the GNOME top bar menu.
It uses the `ddccontrol.DDCControl` D-Bus service and controls the standard VCP
brightness control (`0x10`) on the first detected supported monitor.

## Install

```sh
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/ddccontrol-menu@ddccontrol"
mkdir -p "$EXT_DIR"
cp metadata.json extension.js "$EXT_DIR"/
```

Then restart GNOME Shell (`Alt+F2`, type `r`, Enter on Xorg) or log out/log in,
and enable the extension:

```sh
gnome-extensions enable ddccontrol-menu@ddccontrol
```

## Notes

- The system D-Bus service `ddccontrol.DDCControl` must be installed and running.
- Non-root users are allowed to call the D-Bus API by default project policy.
- The extension is intentionally minimal and can be used as a base for richer
  GNOME integration.
