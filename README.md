[![Build Status][ci-badge]][ci]

[ci-badge]: https://github.com/ddccontrol/ddccontrol/actions/workflows/ci.yml/badge.svg?branch=master
[ci]: https://github.com/ddccontrol/ddccontrol/actions/workflows/ci.yml


# DDC/CI control

DDCcontrol is a software used to control monitor parameters, like brightness, contrast, RGB color levels and others.

DDCcontrol consists of:

* `ddccontrol` - command-line tool for monitor parameters control
* `gddccontrol` - GUI tool for monitor parameters control

DDCcontrol communicates with monitors from userspace through the Linux
`i2c-dev` interface (`/dev/i2c-*`). AMD ADL and legacy direct PCI backends have
been removed. For directly connected displays, this uses the same kernel
userspace I2C path as `ddcutil`; USB-connected DDC/CI displays are not supported
yet.

## Installation

The most convenient way to install DDCcontrol is to use packages from official distribution repositories.

Manual installation is more complicated, but contains latest version of software and more monitor profiles.

### Installation from official packages

DDCcontrol tools, `ddccontrol` and `gddccontrol` can be installed from official distribution repositories with following command:

* on Ubuntu/Debian: `sudo apt install ddccontrol gddccontrol ddccontrol-db i2c-tools`
* on Fedora: `sudo dnf install ddccontrol ddccontrol-gtk`
* on openSUSE: `sudo zypper in ddccontrol i2c-tools`

You might need to restart your system after installing `i2c-tools`.

### Installation from sources

Install build dependencies:

* on Ubuntu: `sudo apt install intltool i2c-tools libxml2-dev libgtk3.0-dev liblzma-dev`
* on Solus: `sudo eopkg install -c system.devel`  
  `sudo eopkg install autoconf automake intltool i2c-tools m4 diffutils libtool-devel xz-devel libxml2-devel libgtk-3-devel`
* on others: install autotools, intltool, i2c-tools, libxml2 development files,
  GTK 3 development files and xz/lzma development files using your
  distribution's package manager.

Clone, build and install built version:

```shell
git clone https://github.com/ddccontrol/ddccontrol.git
cd ddccontrol
./autogen.sh
./configure --prefix=/usr/local/ --sysconfdir=/etc --libexecdir=/usr/local/lib
make
sudo make install
```

Monitor database is required for proper functionality. See for [ddccontrol-db installation](https://github.com/ddccontrol/ddccontrol-db#installation).

## Contributing to the Monitor Database

Follow the instructions on https://github.com/ddccontrol/ddccontrol-db/blob/master/doc/how-to-add-a-monitor.md for inclusion. 
It often merely involves adapting a few standard capabilities as many [pull requests](https://github.com/ddccontrol/ddccontrol-db/pulls) show.


## Usage

### From GUI using gddccontrol

`gddccontrol` is a graphical utility for monitor configuration. It is called **Monitor Settings** in list of applications.

DDCcontrol needs readable and writable `/dev/i2c-*` devices. Most
distributions provide this through the `i2c-dev` kernel module and an `i2c`
group. Following configuration is needed to allow a non-root user to use
`gddccontrol`:

```shell
sudo adduser $USER i2c
sudo /bin/sh -c 'echo i2c-dev >> /etc/modules'
```

Utility can launched directly from commandline:

```shell
sudo gddccontrol
```

`gddccontrol` uses standard GTK3 widgets and follows the configured GTK theme.
To try the GTK high-contrast dark theme for one launch, run:

```shell
GTK_THEME=HighContrastInverse gddccontrol
```

If you need to run `gddccontrol` through `sudo`, preserve the theme override
with `env`:

```shell
sudo env GTK_THEME=HighContrastInverse gddccontrol
```

For GNOME Shell top-bar menu control, see the example extension in
`contrib/gnome-shell-extension`.

### From command line using ddccontrol

`ddccontrol` allows monitor configuration directly from commandline. To probe I2C devices to find monitor buses use:

```shell
sudo ddccontrol -p
```

To read value of control `0x10` (brightness on VESA compliant monitors) for device `dev:/dev/i2c-4`:

```shell
sudo ddccontrol -r 0x10 dev:/dev/i2c-4
```

To set value of control `0x10` (brightness on VESA compliant monitors) to `75` for device `dev:/dev/i2c-4`:

```shell
sudo ddccontrol -r 0x10 -w 75 dev:/dev/i2c-4

# Save current monitor settings to non-volatile memory (if supported)
sudo ddccontrol -r 0x10 -w 75 -s dev:/dev/i2c-4
```

See `ddccontrol -h` for more information.

## Troubleshooting

### NVIDIA proprietary driver — I2C/DDC not working over DisplayPort or HDMI

The NVIDIA proprietary driver sometimes fails to expose `/dev/i2c-*` devices for
DDC/CI communication, causing `ddccontrol -p` to find no monitors (or to return
I2C errors) when connected via DisplayPort or HDMI.

The fix is to add an Xorg configuration snippet that enables software I2C in the
NVIDIA driver.  A ready-made configuration file is shipped with DDCcontrol at
`$(datadir)/ddccontrol/90-nvidia-i2c.conf` (typically
`/usr/share/ddccontrol/90-nvidia-i2c.conf` after installation).  Copy it into
place and restart your X session:

```shell
sudo cp /usr/share/ddccontrol/90-nvidia-i2c.conf /etc/X11/xorg.conf.d/
```

If you built from source without installing, you can copy the file directly from
the source tree:

```shell
sudo cp data/90-nvidia-i2c.conf /etc/X11/xorg.conf.d/
```

After restarting X, `ddccontrol -p` should detect your monitors normally.

See the [NVIDIA developer forum thread](https://forums.developer.nvidia.com/t/gddccontrol-issues-with-nvidia-drivers-i2c-monitor-display-ddc-dp-hdmi-failing/30427)
for background on this issue.

## License

The project is licensed under `GNU General Public License v2.0` license. See [COPYING](COPYING) for details.
