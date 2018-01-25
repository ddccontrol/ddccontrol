# DDCControl D-Bus daemon

## Configuration

Setup D-Bus busconfig for ddccontrol.DDCControl. Warning - probably restarts display manager. Run following under root:

```
cp src/daemon/busconfig.conf /etc/dbus-1/system.d/ddccontrol.DDCControl.conf
systemctl dbus restart
```

## Usage

Run service as root:

```
./src/daemon/service
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
