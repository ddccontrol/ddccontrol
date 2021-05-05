# Changelog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [0.5.2] - 2021-05-05

### Changed

- Build intel810.c and sis.c only when sys/io.h is available (#94)
- Fixed building with autoconf-2.71 (#98)

### Added

- Update README to add Solus distribution build dependencies (#93)
- Add debian instructions (#96)

## [0.5.1] - 2021-01-011

### Changed

- Fixed application version

## [0.5.0] - 2021-01-05

### Added

-   liblzma-dev to the list of required packages to build from source
-   src folder to include reference paths
-   openSUSE installation instructions
-   Information about Fedora package into README
-   TravisCI: add automatic code-format check
-   License headers to new scripts
-   ddccontrol CLI argument `-l` to load profiles created in GUI
-   ddccontrol CLI arg. `-W` to relatively change ctrl value
-   D-Bus daemon to perform HW access under root
-   D-Bus client execution mode to ddccontrol (default mode, if available)
-   systemd service to launch daemon
-   support to disable daemon use by environment variable
-   valgrind tests and fixed memory leaks

### Changed

-   fix gcc10 compilation
-   Fixes building in separate build folder, daemon/dbus_client.h was referenced in main.c directly.
-   remove AUTOMAKE_OPTIONS from configure.ac
-   Setting AUTOMAKE_OPTIONS=xxx is done in a Makefile.am, not configure.ac, where options are set via AM_INIT_AUTOMAKE.
-   Lots of code quality improvements
-   Code Format: using Linux kernel coding style
-   ddccontrol shows warning message without blinking
-   gddccontrol uses D-Bus daemon to access HW devices

## [0.4.4] - 2018-04-26

### Added

-   changelog in _Keep a Changelog_ style

### Changed

-   show warning message without blinking
-   install binary ddcpci to pkglibexec directory (it's not run by users)
-   fixed build with custom CFLAGS
-   fixed configure.ac to use `$PKGCONFIG` set by `PKG_PROG_PKG_CONFIG` macro

## 0.4.3 - 2017-12-28

### Removed

-   original changelog written in GNU style, because it wasn't maintained

[unreleased]: https://github.com/ddccontrol/ddccontrol/compare/0.4.4...master
[0.4.4]: https://github.com/ddccontrol/ddccontrol/compare/0.4.3...0.4.4
