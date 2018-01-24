# DDCControl D-Bus daemon

## Usage

Run service: 

```
./src/daemon/service
```

Test method calls:

```
gdbus call --session --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.GetMonitors
gdbus call --session --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.GetControl 'dev:/dev/i2c-3' 0x12
gdbus call --session --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl --method ddccontrol.DDCControl.SetControl 'dev:/dev/i2c-3' 0x12 25
```

Test signals:

```
gdbus monitor --session --dest ddccontrol.DDCControl --object-path=/ddccontrol/DDCControl 
```
