import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import * as Util from 'resource:///org/gnome/shell/misc/util.js';
import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';

const DDC_DEST = 'ddccontrol.DDCControl';
const DDC_PATH = '/ddccontrol/DDCControl';
const DDC_IFACE = 'ddccontrol.DDCControl';
const BRIGHTNESS_CTRL = 0x10;
const STEP = 10;

function _call(method, params) {
    return new Promise((resolve, reject) => {
        Gio.DBus.system.call(
            DDC_DEST,
            DDC_PATH,
            DDC_IFACE,
            method,
            params,
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null,
            (_connection, result) => {
                try {
                    resolve(Gio.DBus.system.call_finish(result));
                } catch (e) {
                    reject(e);
                }
            }
        );
    });
}

async function _getMonitors() {
    const reply = await _call('GetMonitors', null);
    return reply.deepUnpack();
}

async function _getControl(device, control) {
    const reply = await _call('GetControl', new GLib.Variant('(su)', [device, control]));
    return reply.deepUnpack();
}

async function _setControl(device, control, value) {
    await _call('SetControl', new GLib.Variant('(suu)', [device, control, value]));
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

        // `supported` is D-Bus type a(y); each element unpacks to a single-byte
        // tuple [byte], so the supported flag is at index [0].
        let chosen = null;
        for (let i = 0; i < devices.length; i++) {
            const flag = Array.isArray(supported[i]) ? supported[i][0] : supported[i];
            if (flag) {
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

export default class DDCControlMenuExtension extends Extension {
    enable() {
        this._indicator = new DDCIndicator();
        Main.panel.addToStatusArea('ddccontrol-menu', this._indicator, 0, 'right');
    }

    disable() {
        if (this._indicator) {
            this._indicator.destroy();
            this._indicator = null;
        }
    }
}
