localedir = $(datadir)/locale
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/lib -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddccontrol.la ../daemon/libddccontrol_dbus_client.la

bin_PROGRAMS = ddccontrol
ddccontrol_SOURCES = main.c printing.c
ddccontrol_LDFLAGS = $(LIBXML2_LIBS) $(LIBINTL)
AM_CFLAGS = $(LIBXML2_CFLAGS)

