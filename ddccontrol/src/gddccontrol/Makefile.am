localedir = $(datadir)/locale
INCLUDES = -I../lib -I.. -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddccontrol.la

bin_PROGRAMS = gddccontrol
gddccontrol_SOURCES = main.c notebook.c notebook.h gprofile.c fspatterns.c
gddccontrol_LDFLAGS = $(GNOME_LDFLAGS)
AM_CFLAGS = $(GNOME_CFLAGS)


desktopdir = $(datadir)/applications
Desktop_in_files = gddccontrol.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

