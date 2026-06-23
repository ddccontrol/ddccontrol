SUBDIRS = data src po man $(DOC)
DIST_SUBDIRS = data src po man doc

EXTRA_DIST = config.rpath intltool-extract.in intltool-merge.in intltool-update.in \
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
	crates/ddccontrol-db/Cargo.toml \
	crates/ddccontrol-db/README.md \
	crates/ddccontrol-db/src/lib.rs
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
DISTCHECK_CONFIGURE_FLAGS = --enable-doc

distclean-local:
	rm -f po/.intltool-merge-cache.lock
	rm -rf target

ACLOCAL_AMFLAGS = -I m4
