localedir = $(datadir)/locale
INCLUDES = -I../lib -I.. -DLOCALEDIR=\"$(localedir)\"

EXTRA_DIST = gddccontrol.desktop.in gddccontrol.png gddccontrol-bluecurve.png

LDADD = ../lib/libddccontrol.la

bin_PROGRAMS = gddccontrol
gddccontrol_SOURCES = main.c notebook.c notebook.h gprofile.c fspatterns.c
gddccontrol_LDFLAGS = $(GNOME_LDFLAGS)
AM_CFLAGS = $(GNOME_CFLAGS)


desktopdir = $(datadir)/applications
Desktop_in_files = gddccontrol.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = gddccontrol.png

install-data-local:
	-@INSTALL@ -m 644 gddccontrol-bluecurve.png $(DESTDIR)/$(datadir)/icons/Bluecurve/48x48/apps/gddccontrol.png

@INTLTOOL_DESKTOP_RULE@

