localedir = $(datadir)/locale
AM_CFLAGS = $(LIBXML2_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DLOCALEDIR=\"$(localedir)\"

pkginclude_HEADERS = conf.h monitor_db.h ddcci.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ddccontrol.pc

$(pkgconfig_DATA): $(srcdir)/ddccontrol.pc.in $(top_builddir)/config.status

modulesdir = $(libdir)/modules-load.d/
modules_DATA = ddccontrol-i2c-dev.conf
EXTRA_DIST = $(modules_DATA)

lib_LTLIBRARIES = libddccontrol.la

RUST_DB_LIB = $(top_builddir)/target/release/libddccontrol_db.a
RUST_DB_DEPS = \
	$(top_srcdir)/Cargo.toml \
	$(top_srcdir)/Cargo.lock \
	$(top_srcdir)/crates/ddccontrol-caps/Cargo.toml \
	$(top_srcdir)/crates/ddccontrol-caps/src/lib.rs \
	$(top_srcdir)/crates/ddccontrol-caps/fixtures/basic_lcd.caps \
	$(top_srcdir)/crates/ddccontrol-caps/fixtures/compact_vcp.caps \
	$(top_srcdir)/crates/ddccontrol-caps/fixtures/db_patch_add_remove.caps \
	$(top_srcdir)/crates/ddccontrol-caps/fixtures/fallback_crt.caps \
	$(top_srcdir)/crates/ddccontrol-caps/fixtures/nested_values.caps \
	$(top_srcdir)/crates/ddccontrol-db/Cargo.toml \
	$(top_srcdir)/crates/ddccontrol-db/src/lib.rs

$(RUST_DB_LIB): $(RUST_DB_DEPS)
	@$(CARGO) --version >/dev/null 2>&1 || { \
		echo "error: cargo was found by configure but could not run."; \
		echo "If this is rustup's cargo shim, run 'rustup default stable' or set CARGO to a working cargo binary and rerun configure."; \
		exit 1; \
	}
	cd $(top_srcdir) && DDCONTROL_DATADIR="$(datadir)/ddccontrol-db" CARGO_TARGET_DIR=$(abs_top_builddir)/target $(CARGO) build --release -p ddccontrol-db $(RUST_GETTEXT_FEATURE)

libddccontrol_la_SOURCES = ddcci.c ddcci.h monitor_db.h \
   i2c-dev.h conf.c conf.h internal.h \
   monitor_db_internal.h

libddccontrol_la_LDFLAGS = -version-info 1:0:0
libddccontrol_la_LIBADD = $(RUST_DB_LIB) $(LIBXML2_LIBS)

check_PROGRAMS = test_issueurl test_parse_caps test_edid test_abi_layout
TESTS = $(check_PROGRAMS)

check-local:
	@$(CARGO) --version >/dev/null 2>&1 || { \
		echo "error: cargo was found by configure but could not run."; \
		echo "If this is rustup's cargo shim, run 'rustup default stable' or set CARGO to a working cargo binary and rerun configure."; \
		exit 1; \
	}
	cd $(top_srcdir) && CARGO_TARGET_DIR=$(abs_top_builddir)/target $(CARGO) test --workspace $(RUST_GETTEXT_FEATURE)

test_issueurl_SOURCES = tests/test_issueurl.c issueurl.c urlencode.c
test_issueurl_LDADD = libddccontrol.la $(LIBXML2_LIBS)

test_parse_caps_SOURCES = tests/test_parse_caps.c
test_parse_caps_LDADD = libddccontrol.la $(LIBXML2_LIBS)

test_edid_SOURCES = tests/test_edid.c
test_edid_LDADD = libddccontrol.la $(LIBXML2_LIBS)

test_abi_layout_SOURCES = tests/test_abi_layout.c
