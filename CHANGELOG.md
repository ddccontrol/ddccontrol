# Changelog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- D-Bus daemon to perform HW access under root
- D-Bus client execution mode to ddccontrol (default mode, if available)
- systemd service to launch daemon

### Changed
- ddccontrol shows warning message without blinking
- gddccontrol uses D-Bus daemon to access HW devices

## [0.4.4] - 2018-04-26
### Added
- changelog in *Keep a Changelog* style

### Changed
- show warning message without blinking
- install binary ddcpci to pkglibexec directory (it's not run by users)
- fixed build with custom CFLAGS
- fixed configure.ac to use `$PKGCONFIG` set by `PKG_PROG_PKG_CONFIG` macro

## 0.4.3 - 2017-12-28
### Removed
- original changelog written in GNU style, because it wasn't maintained

[Unreleased]: https://github.com/ddccontrol/ddccontrol/compare/0.4.4...master
[0.4.4]: https://github.com/ddccontrol/ddccontrol/compare/0.4.3...0.4.4
