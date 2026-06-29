# Changelog

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [3.2.0](https://github.com/ddccontrol/ddccontrol/compare/3.1.2...3.2.0) (2026-06-29)


### Features

* harden Rust ABI bridge ([#302](https://github.com/ddccontrol/ddccontrol/issues/302)) ([27e16b5](https://github.com/ddccontrol/ddccontrol/commit/27e16b5a2521c3e7afdcc44665fc1f58fbbe8ba5))


### Bug Fixes

* handle real database compatibility cases ([#304](https://github.com/ddccontrol/ddccontrol/issues/304)) ([fe666ab](https://github.com/ddccontrol/ddccontrol/commit/fe666abcfa66e96811b80670557a1d12e70cf8da))


### Build System

* upload vendored release sources ([#305](https://github.com/ddccontrol/ddccontrol/issues/305)) ([4e181c6](https://github.com/ddccontrol/ddccontrol/commit/4e181c6fff1c58a1115d0ed7b1ec6a915de4618c))

## [3.1.2](https://github.com/ddccontrol/ddccontrol/compare/3.1.1...3.1.2) (2026-06-24)


### Bug Fixes

* keep distcheck systemd install under prefix ([#300](https://github.com/ddccontrol/ddccontrol/issues/300)) ([d741afd](https://github.com/ddccontrol/ddccontrol/commit/d741afd085af45f91519bb22976345defe08b6ac))

## [3.1.1](https://github.com/ddccontrol/ddccontrol/compare/3.1.0...3.1.1) (2026-06-23)


### Bug Fixes

* repair release source distribution build ([#297](https://github.com/ddccontrol/ddccontrol/issues/297)) ([353e680](https://github.com/ddccontrol/ddccontrol/commit/353e680bf0a9d62f01b16b18cb8ed3e76d09fa10))

## [3.1.0](https://github.com/ddccontrol/ddccontrol/compare/3.0.0...3.1.0) (2026-06-23)


### Features

* **gddccontrol:** show extended EDID information ([#280](https://github.com/ddccontrol/ddccontrol/issues/280)) ([4f7be89](https://github.com/ddccontrol/ddccontrol/commit/4f7be8911e0dcf3536fa308d490603d624da1ece))


### Bug Fixes

* hide engineering VCP controls from dumps ([#295](https://github.com/ddccontrol/ddccontrol/issues/295)) ([0c1ec3c](https://github.com/ddccontrol/ddccontrol/commit/0c1ec3c40b44d5f76ba6ae592564e89fdac9c960))
* improve doc build dependency handling ([#293](https://github.com/ddccontrol/ddccontrol/issues/293)) ([e88f655](https://github.com/ddccontrol/ddccontrol/commit/e88f655368283ef3208838a52d33e252349748dc))


### Documentation

* remove static supported monitors list ([#277](https://github.com/ddccontrol/ddccontrol/issues/277)) ([048c4c0](https://github.com/ddccontrol/ddccontrol/commit/048c4c0c6d2a5f76e6043785352db19faf82d815))


### Code Refactoring

* **parser:** replace monitor DB XML and CAPS parsers with Rust ([#257](https://github.com/ddccontrol/ddccontrol/issues/257)) ([c695559](https://github.com/ddccontrol/ddccontrol/commit/c695559fd137cb3706df3de52c14e31c25702b0c))

## [3.0.0](https://github.com/ddccontrol/ddccontrol/compare/2.2.0...3.0.0) (2026-06-11)


### ⚠ BREAKING CHANGES

* AMD ADL and legacy PCI backends were removed.

### Features

* **gddccontrol:** add F5 refresh monitor list shortcut ([#261](https://github.com/ddccontrol/ddccontrol/issues/261)) ([44e6afd](https://github.com/ddccontrol/ddccontrol/commit/44e6afdca3117af10fd0a9a60fbe30cd641050f6))
* remove AMD ADL and legacy PCI backends ([#256](https://github.com/ddccontrol/ddccontrol/issues/256)) ([83add99](https://github.com/ddccontrol/ddccontrol/commit/83add993d4ad038aaddb45ee55869b0c292159ab))


### Bug Fixes

* Add escape syntax to un-break missing XML message ([#270](https://github.com/ddccontrol/ddccontrol/issues/270)) ([1eb0679](https://github.com/ddccontrol/ddccontrol/commit/1eb067999b3d4c7a035f9fb09652cee3fc8df144))
* **gddccontrol:** honor DDCCONTROL_NO_DAEMON ([#262](https://github.com/ddccontrol/ddccontrol/issues/262)) ([abd1cac](https://github.com/ddccontrol/ddccontrol/commit/abd1cac4741e0ab70132d301b402d0fe05b5bb38))
* **gddccontrol:** improve GTK high-contrast accessibility ([#264](https://github.com/ddccontrol/ddccontrol/issues/264)) ([7b098de](https://github.com/ddccontrol/ddccontrol/commit/7b098ded35d8d56bb44633d658c5daf7175867c7))
* ship NVIDIA xorg I2C config and update DDC/CI docs ([#255](https://github.com/ddccontrol/ddccontrol/issues/255)) ([cf3b79d](https://github.com/ddccontrol/ddccontrol/commit/cf3b79ddca94670fb57761b9ac0d529e07bbc81e))


### Documentation

* credit contributors in copyright notices ([#266](https://github.com/ddccontrol/ddccontrol/issues/266)) ([967c8b5](https://github.com/ddccontrol/ddccontrol/commit/967c8b5a4e3eb9cf3551710560ecd9ee326ca385))
* credit copyright contributors ([#268](https://github.com/ddccontrol/ddccontrol/issues/268)) ([f1f6e00](https://github.com/ddccontrol/ddccontrol/commit/f1f6e009c33cb728dc9ce479efb3e597dd000352))
* replace SECURITY.md placeholder with accurate policy ([#260](https://github.com/ddccontrol/ddccontrol/issues/260)) ([5bb54b6](https://github.com/ddccontrol/ddccontrol/commit/5bb54b6655577a1f07a1bdb26adf55c4cd70797a))
* update docs for userspace I2C-only backend ([#265](https://github.com/ddccontrol/ddccontrol/issues/265)) ([6166240](https://github.com/ddccontrol/ddccontrol/commit/61662403d3d1c32565ef7b2957435cb3e0e07471))


### Chores

* release 3.0.0 ([7c5cba1](https://github.com/ddccontrol/ddccontrol/commit/7c5cba170b42e25c2b6786f73228ab139c4971e1))

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

### Added

-   Support for Intel Mobile 945GME Express Graphics Controller.
-   Support for AMD ADL as a backend.
-   Support for FreeBSD.
-   Convenient `.pc` file.

### Changed

-   Made desktop file conform to standards.
-   Used custom icon instead of depending on an obsolete third-party icon.
-   Various build, spelling, and buffer overflow fixes.

### Removed

-   original changelog written in GNU style, because it wasn't maintained

## 0.4.2 - 2006-07-30

### Added

-   Support for binary data in CAPS.
-   More I2C busses to Intel 810-like chips.
-   Manual pages.

### Fixed

-   gcc 4.0 and 4.1 compilation warnings.

## 0.4.1 - 2006-03-10

### Fixed

-   Compilation error in Fedora Core 4, which may affect other distributions.

## 0.4 - 2006-03-08

### Added

-   New database version (3), supporting generic profiles for monitors.
-   Support for VIA and SiS chipsets, thanks to Johannes Deisenhofer.

### Fixed

-   Memory leaks found using valgrind.
-   Various compilation errors.

## 0.3 - 2005-11-14

### Added

-   Chinese translation, thanks to "waq_cn".
-   Gnome icon, thanks to "lekma" on ddccontrol-users.
-   New Gnome Panel Applet to switch between monitor profiles, thanks to
    Christian Schilling.
-   New fullscreen patterns.

### Changed

-   Used GTK/GDK high-level API to determine current screen instead of Xinerama
    API.
-   Sped up gddccontrol loading by caching monitor list.

## 0.2 - 2005-08-13

### Added

-   Restore button for each control in gddccontrol.
-   Refresh button in gddccontrol, and refresh all controls when needed.
-   Support for profiles to save some controls values and restore them later.
-   Fullscreen pattern to adjust brightness and contrast.
-   New database version, requiring an upgrade to ddccontrol-db.

## 0.1.3 - 2005-07-15

### Added

-   Support for i810/i815/i830/i845/i855/i865/i915/i945 chipsets, thanks to
    Chernyavskyy Valentin for testing.
-   Russian translation, thanks to Sergei Epiphanov.

## 0.1.2 - 2005-06-08

### Fixed

-   Compilation error in Fedora Core 3.

## 0.1.1 - 2005-06-07

### Added

-   Support for newest nVidia cards, such as GeForce 6200 TC.
-   Static ddcpci build for security reasons.
-   Status messages when gddccontrol loads.

### Changed

-   Displayed fewer error messages when devices are not usable.
-   Other minor fixes.

### Fixed

-   Compile error with latest kernel headers.

## 0.1 - 2005-03-29

### Added

-   Initial release.

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
