# DDCControl D-Bus daemon

## Installation

Build and install the daemon:

```
cd src/daemon
make
sudo make install
```

D-Bus restart might be needed to apply bus config changes. Warning - probably restarts display manager. Run following under root:

```
systemctl dbus restart
```

## Usage

Service should be started automatically on D-Bus calls. If not, run:

```
sudo systemctl start ddccontrol
```

Test method calls as regular user:

```
gdbus call --system --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.GetMonitors
gdbus call --system --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.GetControl 'dev:/dev/i2c-3' 0x12
gdbus call --system --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.SetControl 'dev:/dev/i2c-3' 0x12 25
```

Test signals:

```
gdbus monitor --system --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl 
```

## Resources

* [D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-specification.htm)
* [D-Bus API Design Guidelines](https://dbus.freedesktop.org/doc/dbus-api-design.html)