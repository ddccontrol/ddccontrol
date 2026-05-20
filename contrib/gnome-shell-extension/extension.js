'use strict';

const { Gio, GLib, GObject, St } = imports.gi;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

const DDC_DEST = 'ddccontrol.DDCControl';
const DDC_PATH = '/ddccontrol/DDCControl';
const DDC_IFACE = 'ddccontrol.DDCControl';
const BRIGHTNESS_CTRL = 0x10;
const STEP = 10;

function _getMonitors() {
    return new Promise((resolve, reject) => {
        Gio.DBus.system.call(
            DDC_DEST,
            DDC_PATH,
            DDC_IFACE,
            'GetMonitors',
            null,
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null,
            (_connection, result) => {
                try {
                    const value = Gio.DBus.system.call_finish(result);
                    resolve(value.deepUnpack());
                } catch (e) {
                    reject(e);
                }
            }
        );
    });
}

function _getControl(device, control) {
    return new Promise((resolve, reject) => {
        Gio.DBus.system.call(
            DDC_DEST,
            DDC_PATH,
            DDC_IFACE,
            'GetControl',
            new GLib.Variant('(su)', [device, control]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null,
            (_connection, result) => {
                try {
                    const value = Gio.DBus.system.call_finish(result);
                    resolve(value.deepUnpack());
                } catch (e) {
                    reject(e);
                }
            }
        );
    });
}

function _setControl(device, control, value) {
    return new Promise((resolve, reject) => {
        Gio.DBus.system.call(
            DDC_DEST,
            DDC_PATH,
            DDC_IFACE,
            'SetControl',
            new GLib.Variant('(suu)', [device, control, value]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null,
            (_connection, result) => {
                try {
                    Gio.DBus.system.call_finish(result);
                    resolve();
                } catch (e) {
                    reject(e);
                }
            }
        );
    });
}

const DDCIndicator = GObject.registerClass(
class DDCIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.0, 'DDCControl Menu');

        this.add_child(new St.Icon({
            icon_name: 'display-brightness-symbolic',
            style_class: 'system-status-icon',
        }));

        let brighter = new PopupMenu.PopupMenuItem('Brightness +10');
        brighter.connect('activate', () => {
            this._changeBrightness(STEP).catch(e => Main.notifyError('DDCControl', `${e}`));
        });
        this.menu.addMenuItem(brighter);

        let dimmer = new PopupMenu.PopupMenuItem('Brightness -10');
        dimmer.connect('activate', () => {
            this._changeBrightness(-STEP).catch(e => Main.notifyError('DDCControl', `${e}`));
        });
        this.menu.addMenuItem(dimmer);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        let open = new PopupMenu.PopupMenuItem('Open Monitor Settings');
        open.connect('activate', () => Util.spawn(['gddccontrol']));
        this.menu.addMenuItem(open);
    }

    async _changeBrightness(delta) {
        const [devices, supported] = await _getMonitors();
        if (!devices || devices.length === 0)
            throw new Error('No DDC-capable monitor found');

        let chosen = null;
        for (let i = 0; i < devices.length; i++) {
            if (supported[i]) {
                chosen = devices[i];
                break;
            }
        }
        if (chosen === null)
            throw new Error('No supported monitor found');

        const [result, value, maximum] = await _getControl(chosen, BRIGHTNESS_CTRL);
        if (result < 0)
            throw new Error(`Failed to read brightness (${result})`);

        let next = value + delta;
        if (next < 0)
            next = 0;
        if (next > maximum)
            next = maximum;

        await _setControl(chosen, BRIGHTNESS_CTRL, next);
    }
});

class Extension {
    constructor() {
        this._indicator = null;
    }

    enable() {
        this._indicator = new DDCIndicator();
        Main.panel.addToStatusArea('ddccontrol-menu', this._indicator, 0, 'right');
    }

    disable() {
        if (this._indicator !== null) {
            this._indicator.destroy();
            this._indicator = null;
        }
    }
}

function init() {
    return new Extension();
}
