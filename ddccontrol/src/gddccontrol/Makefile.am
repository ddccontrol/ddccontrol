localedir = $(datadir)/locale
INCLUDES = -I../lib -I.. -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddcci.a

bin_PROGRAMS = gddccontrol
gddccontrol_SOURCES = main.c notebook.c notebook.h gprofile.c fspatterns.c
gddccontrol_LDFLAGS = $(GNOME_LDFLAGS)
AM_CFLAGS = $(GNOME_CFLAGS)

