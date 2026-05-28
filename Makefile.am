SUBDIRS = data src po man $(DOC)
DIST_SUBDIRS = data src po man doc

EXTRA_DIST = config.rpath intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
DISTCHECK_CONFIGURE_FLAGS = --enable-doc

distclean-local:
	rm -f po/.intltool-merge-cache.lock

ACLOCAL_AMFLAGS = -I m4
