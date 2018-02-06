localedir = $(datadir)/locale
AM_CPPFLAGS = -I$(top_srcdir)/src/lib -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddccontrol.la

bin_PROGRAMS = ddccontrol
ddccontrol_SOURCES = main.c dbus_client.c printing.c dbus_interface.h dbus_interface.c
ddccontrol_LDFLAGS = $(LIBXML2_LDFLAGS) $(LIBINTL)
AM_CFLAGS = $(LIBXML2_CFLAGS)

BUILT_SOURCES = dbus_interface.h dbus_interface.c
noinst_HEADERS = $(BUILT_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

main.c: dbus_interface.h dbus_interface.c

dbus_interface.h dbus_interface.c: ../daemon/ddccontrol.DDCControl.xml
	gdbus-codegen --interface-prefix ddccontrol --generate-c-code dbus_interface $<
