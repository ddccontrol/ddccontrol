#!/usr/bin/python3
# Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

"""Run the scanner against a fake ddccontrol service on a private test bus."""

import os
from pathlib import Path
import subprocess
import sys
import threading

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib


scenario, binary, *arguments = sys.argv[1:]
interface_xml = (
    Path(__file__).resolve().parents[3] / "src/daemon/ddccontrol.DDCControl.xml"
).read_text()
interface = Gio.DBusNodeInfo.new_for_xml(interface_xml).interfaces[0]
connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
connection.call_sync(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "RequestName",
    GLib.Variant("(su)", ("ddccontrol.DDCControl", 0)),
    GLib.VariantType.new("(u)"),
    Gio.DBusCallFlags.NONE,
    5000,
    None,
)
unexpected_calls = []


def handle_call(_connection, _sender, _path, _interface, method, parameters, reply):
    try:
        if method == "RescanMonitors":
            devices = ["dev:/dev/i2c-4", "dev:/dev/i2c-5"]
            names = ["Fixture Monitor", "Unsupported Panel"]
            supported = [(1,), (0,)]
            if scenario == "multiple":
                supported[1] = (1,)
            if scenario == "none":
                supported[0] = (0,)
            if scenario == "bad_lists":
                names.pop()
            reply.return_value(
                GLib.Variant("(asa(y)asa(y))", (devices, supported, names, [(128,), (128,)]))
            )
        elif method == "OpenMonitor":
            pnp_id = "../evil" if scenario == "invalid_pnp" else "DEL1234"
            capabilities = "(type(lcd)model(Fixture) vcp(10 12 16 e1))"
            if scenario == "bad_caps":
                capabilities = "(vcp(10"
            fields = (capabilities, pnp_id) if scenario == "legacy" else (pnp_id, capabilities)
            reply.return_value(GLib.Variant("(ss)", fields))
        elif method == "GetEdid":
            if scenario in ("no_edid", "legacy"):
                reply.return_dbus_error("org.freedesktop.DBus.Error.UnknownMethod", "GetEdid")
                return
            edid = bytearray(128)
            edid[:8] = bytes((0, 255, 255, 255, 255, 255, 255, 0))
            edid[8:12] = bytes((0x10, 0xAC, 0x34, 0x12))
            edid[0x14] = 0x80
            edid[0x36:0x3B] = bytes((0, 0, 0, 0xFC, 0))
            edid[0x3B:0x48] = b"Test & Screen"
            edid[127] = (-sum(edid)) & 255
            reply.return_value(GLib.Variant("(ay)", (list(edid),)))
        elif method == "GetControl":
            _device, code = parameters.unpack()
            if code >= 0xE0:
                unexpected_calls.append(f"engineering address 0x{code:02x}")
            if scenario == "unreadable":
                result = (-1, 0, 0)
            elif code == 0x10:
                result = (1, 42, 100)
            elif code == 0x16:
                result = (-1, 0, 0)
            else:
                result = (0, 0, 0)
            reply.return_value(GLib.Variant("(iqq)", result))
        else:
            unexpected_calls.append(method)
            reply.return_dbus_error("org.example.TestUnexpectedCall", method)
    except Exception as error:
        unexpected_calls.append(str(error))
        reply.return_dbus_error("org.example.TestFixtureFailed", str(error))


registration = connection.register_object(
    "/ddccontrol/DDCControl", interface, handle_call, None, None
)
loop = GLib.MainLoop()
exit_code = 99


def run_scanner():
    global exit_code
    try:
        environment = dict(os.environ)
        # This address belongs to dbus-run-session; no real system bus or
        # monitor is accessible to the scanner through this fixture.
        environment["DBUS_SYSTEM_BUS_ADDRESS"] = environment["DBUS_SESSION_BUS_ADDRESS"]
        result = subprocess.run(
            [binary, *arguments], env=environment, capture_output=True, timeout=30
        )
        sys.stdout.buffer.write(result.stdout)
        sys.stderr.buffer.write(result.stderr)
        exit_code = result.returncode
    except Exception as error:
        print(f"D-Bus test fixture failed: {error}", file=sys.stderr)
    finally:
        GLib.idle_add(loop.quit)


worker = threading.Thread(target=run_scanner)
worker.start()
loop.run()
worker.join()
connection.unregister_object(registration)
if unexpected_calls:
    print(f"Unexpected calls: {unexpected_calls}", file=sys.stderr)
    exit_code = 99
sys.exit(exit_code)
