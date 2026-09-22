SUBDIRS = data src po man $(DOC)
DIST_SUBDIRS = data src po man doc

EXTRA_DIST = config.rpath intltool-extract.in intltool-merge.in intltool-update.in \
	rust-porting.txt \
	Cargo.toml \
	Cargo.lock \
	crates/ddccontrol-caps/Cargo.toml \
	crates/ddccontrol-caps/README.md \
	crates/ddccontrol-caps/fixtures/basic_lcd.caps \
	crates/ddccontrol-caps/fixtures/compact_vcp.caps \
	crates/ddccontrol-caps/fixtures/db_patch_add_remove.caps \
	crates/ddccontrol-caps/fixtures/fallback_crt.caps \
	crates/ddccontrol-caps/fixtures/nested_values.caps \
	crates/ddccontrol-caps/src/lib.rs \
	crates/ddccontrol-caps/tests/parser.rs \
	crates/ddccontrol-edid/Cargo.toml \
	crates/ddccontrol-edid/PIAF.md \
	crates/ddccontrol-edid/README.md \
	crates/ddccontrol-edid/src/lib.rs \
	crates/ddccontrol-edid/tests/parser.rs \
	crates/ddccontrol-profile/Cargo.toml \
	crates/ddccontrol-profile/README.md \
	crates/ddccontrol-profile/src/lib.rs \
	crates/ddccontrol-profile/tests/profile.rs \
	crates/ddccontrol-monitorlist/Cargo.toml \
	crates/ddccontrol-monitorlist/README.md \
	crates/ddccontrol-monitorlist/src/lib.rs \
	crates/ddccontrol-monitorlist/tests/monitorlist.rs \
	crates/ddccontrol-protocol/Cargo.toml \
	crates/ddccontrol-protocol/README.md \
	crates/ddccontrol-protocol/src/lib.rs \
	crates/ddccontrol-protocol/tests/protocol.rs \
	crates/ddccontrol-protocol/fuzz/Cargo.toml \
	crates/ddccontrol-protocol/fuzz/.gitignore \
	crates/ddccontrol-protocol/fuzz/fuzz_targets/protocol.rs \
	crates/ddccontrol-db/Cargo.toml \
	crates/ddccontrol-db/README.md \
	crates/ddccontrol-db/fixtures/compat-db/options.xml \
	crates/ddccontrol-db/fixtures/compat-db/monitor/compat-common.xml \
	crates/ddccontrol-db/fixtures/compat-db/monitor/compat-monitor.xml \
	crates/ddccontrol-db/tests/unit/ffi_layout.rs \
	crates/ddccontrol-db/tests/unit/monitor_db.rs \
	crates/ddccontrol-db/tests/unit/monitor_db_compatibility.rs \
	crates/ddccontrol-db/tests/unit/profile_ffi_layout.rs \
	crates/ddccontrol-db/tests/unit/monitor_list.rs \
	crates/ddccontrol-db/src/monitor_list.rs \
	crates/ddccontrol-db/src/lib.rs \
	crates/ddccontrol-db/src/options.rs \
	crates/ddccontrol-db/src/xml.rs \
	crates/ddccontrol-xml/Cargo.toml \
	crates/ddccontrol-xml/README.md \
	crates/ddccontrol-xml/src/lib.rs \
	crates/ddccontrol-db/src/protocol.rs \
	crates/ddccontrol-scanmonitor/Cargo.toml \
	crates/ddccontrol-scanmonitor/README.md \
	crates/ddccontrol-scanmonitor/src/main.rs \
	crates/ddccontrol-scanmonitor/src/backend.rs \
	crates/ddccontrol-scanmonitor/src/xml.rs \
	crates/ddccontrol-scanmonitor/tests/cli.rs \
	crates/ddccontrol-scanmonitor/tests/dbus.rs \
	crates/ddccontrol-scanmonitor/tests/dbus_service.py \
	crates/ddccontrol-scanmonitor/fixtures/options.xml
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
DISTCHECK_CONFIGURE_FLAGS = --enable-doc --with-systemdsystemunitdir='$${prefix}/lib/systemd/system'

RUST_SCANMONITOR = $(top_builddir)/target/release/ddccontrol-scanmonitor
# Include transitive Rust sources so edits to shared crates rebuild the scanner.
RUST_SCANMONITOR_DEPS = \
	$(top_builddir)/config.status \
	$(top_srcdir)/Cargo.toml \
	$(top_srcdir)/Cargo.lock \
	$(wildcard $(top_srcdir)/crates/*/Cargo.toml) \
	$(wildcard $(top_srcdir)/crates/*/src/*.rs)

all-local: $(RUST_SCANMONITOR)

$(RUST_SCANMONITOR): $(RUST_SCANMONITOR_DEPS)
	cd "$(top_srcdir)" && DDCONTROL_DATADIR="$(datadir)/ddccontrol-db" CARGO_TARGET_DIR="$(abs_top_builddir)/target" $(CARGO) build --release --locked -p ddccontrol-scanmonitor

install-exec-local: $(RUST_SCANMONITOR)
	$(MKDIR_P) "$(DESTDIR)$(bindir)"
	$(INSTALL_PROGRAM) "$(RUST_SCANMONITOR)" "$(DESTDIR)$(bindir)/ddccontrol-scanmonitor$(EXEEXT)"

uninstall-local:
	rm -f "$(DESTDIR)$(bindir)/ddccontrol-scanmonitor$(EXEEXT)"

clean-local:
	rm -f "$(RUST_SCANMONITOR)"

distclean-local:
	rm -f po/.intltool-merge-cache.lock
	rm -rf target

ACLOCAL_AMFLAGS = -I m4
