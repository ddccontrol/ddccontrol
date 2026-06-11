# Changelog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Remove AMD ADL and legacy direct PCI backends.

## [2.2.0] - 2026-05-27

### Added

- GNOME Shell top-bar brightness menu extension example in `contrib/gnome-shell-extension`

### Changed

- Deprecate AMD ADL and legacy direct PCI backends, disable both by default,
  and require explicit configure flags to build them.

### Fixed

- fix drop-down monitor identity bug: non-deterministic directory
  entry reads and inappropriate defaults bollixed the monitor list
  - Sort i2c devices numerically in `ddcci_probe` for consistent ordering
  - Include PnP ID in monitor label when using fallback profile, so monitors
    not in the ddccontrol-db are still identifiable

## [2.0.0] - 2026-05-18

### Added

- Georgian translation
- Security policy (SECURITY.md)
- Dockerfile for consistent build environment
- ARM64 build in GitHub Actions CI matrix
- Clang build in GitHub Actions CI matrix
- Scheduled build of GitHub CI image

### Changed

- Port gddccontrol GUI from GTK2 to GTK3
  - Replace deprecated GTK2 widgets and drawing APIs with GTK3 equivalents
  - Replace `GdkDrawable`/`GdkColor` with `cairo_surface_t`/Cairo color functions
  - Fix use-after-free bug when closing the main window
- Switch unsupported monitor reporting from mailing list to GitHub issues
- Fix `const gchar` usage for values returned by `g_variant_get_fixed_array()`
- Update GitHub Actions workflow to actions/checkout@v6
- Add desktop keywords and service documentation pointer (QA tweaks)

## [1.0.3] - 2024-01-31

### Changed

- Bump version in `configure.ac`

## [1.0.2] - 2024-01-29

### Fixed

- Fixed build with `gcc-14`

## [1.0.1] - 2024-01-24

### Added

- Missing `#include <stdlib.h>` (partial fix for build failure)

### Changed

- Refactor gddccontrol drawing code in preparation for GTK3 migration
  (Cairo/Pango, reference counting, variable name cleanup)

## [1.0.0] - 2023-10-15

Re-tagged release of 0.6.4.

## [0.6.4] - 2023-10-15

### Removed

- Drop GNOME applet

### Fixed

- DBus client: fix compiler warning
- Remove unused variables

### Changed

- README: suggest `/usr/local` for manual (non-package-manager) installations

## [0.6.3] - 2023-09-04

### Fixed

- dbus: set signal argument directions to `"out"`

## [0.6.2] - 2023-07-17

### Added

- Hint in UI directing users to contribute missing monitors to the database

## [0.6.1] - 2022-09-14

### Fixed

- Add missing `return` statement in `dbus_monitor_close` (caused build error
  with `-Werror=return-type`)
- Fix misleading hint about running daemon in foreground
- Fix `modulesdir` in `src/lib/Makefile.am` to respect custom install prefix

### Changed

- Follow autotools recommended practices (use `AC_PROG_MKDIR_P`, approved
  cleaning declarations)
- Centralize version definition

## [0.6.0] - 2021-10-04

### Added

- German translation (forward-ported from Debian patch by Chris Leick)

### Changed

- Update autotools infrastructure

## [0.5.3] - 2021-10-01

### Fixed

- Load `i2c-dev` kernel module at boot via `modules-load.d`
- Link `libddccontrol_dbus_client.so` against `libddccontrol.so` to resolve
  missing symbols (`ddcci_parse_caps`, `ddcci_create_db`, etc.)
- Use `pkg-config` instead of deprecated `xml2-config`

## [0.5.2] - 2021-05-05

### Changed

- Build `intel810.c` and `sis.c` only when `sys/io.h` is available (#94)
- Fixed building with `autoconf-2.71` (#98)

### Added

- Update README to add Solus distribution build dependencies (#93)
- Add debian instructions (#96)

## [0.5.1] - 2021-01-011

### Changed

- Fixed application version

## [0.5.0] - 2021-01-05

### Added

-   `liblzma-dev` to the list of required packages to build from source
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

-   fix `gcc10` compilation
-   Fixes building in separate build folder, `daemon/dbus_client.h` was referenced in `main.c` directly.
-   remove `AUTOMAKE_OPTIONS` from `configure.ac`
-   Setting `AUTOMAKE_OPTIONS=xxx` is done in a `Makefile.am`, not `configure.ac`, where options are set via `AM_INIT_AUTOMAKE`.
-   Lots of code quality improvements
-   Code Format: using Linux kernel coding style
-   ddccontrol shows warning message without blinking
-   gddccontrol uses D-Bus daemon to access HW devices

## [0.4.4] - 2018-04-26

### Added

-   changelog in _Keep a Changelog_ style

### Changed

-   show warning message without blinking
-   install binary `ddcpci` to `pkglibexec` directory (it's not run by users)
-   fixed build with custom CFLAGS
-   fixed configure.ac to use `$PKGCONFIG` set by `PKG_PROG_PKG_CONFIG` macro

## 0.4.3 - 2017-12-28

### Removed

-   original changelog written in GNU style, because it wasn't maintained

[unreleased]: https://github.com/ddccontrol/ddccontrol/compare/2.2.0...master
[2.2.0]: https://github.com/ddccontrol/ddccontrol/compare/2.1.0...2.2.0
[2.0.0]: https://github.com/ddccontrol/ddccontrol/compare/1.0.3...2.0.0
[1.0.3]: https://github.com/ddccontrol/ddccontrol/compare/1.0.2...1.0.3
[1.0.2]: https://github.com/ddccontrol/ddccontrol/compare/1.0.1...1.0.2
[1.0.1]: https://github.com/ddccontrol/ddccontrol/compare/1.0.0...1.0.1
[1.0.0]: https://github.com/ddccontrol/ddccontrol/compare/0.6.3...1.0.0
[0.6.4]: https://github.com/ddccontrol/ddccontrol/compare/0.6.3...0.6.4
[0.6.3]: https://github.com/ddccontrol/ddccontrol/compare/0.6.2...0.6.3
[0.6.2]: https://github.com/ddccontrol/ddccontrol/compare/0.6.1...0.6.2
[0.6.1]: https://github.com/ddccontrol/ddccontrol/compare/0.6.0...0.6.1
[0.6.0]: https://github.com/ddccontrol/ddccontrol/compare/0.5.3...0.6.0
[0.5.3]: https://github.com/ddccontrol/ddccontrol/compare/0.5.2...0.5.3
[0.5.2]: https://github.com/ddccontrol/ddccontrol/compare/0.5.1...0.5.2
[0.5.1]: https://github.com/ddccontrol/ddccontrol/compare/0.5.0...0.5.1
[0.5.0]: https://github.com/ddccontrol/ddccontrol/compare/0.4.4...0.5.0
[0.4.4]: https://github.com/ddccontrol/ddccontrol/compare/0.4.3...0.4.4
