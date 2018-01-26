SUBDIRS = data

AM_CFLAGS = $(LIBXML2_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src/lib
LDADD = ../lib/libddccontrol.la

# install D-Bus API specification for ddccontrol.DDCControl.xml interface
dbusinterfacesdir = $(datadir)/dbus-1/interfaces
dist_dbusinterfaces_DATA = ddccontrol.DDCControl.xml

# install to libexec, because it's not intended to be run by user
libexec_PROGRAMS = ddccontrol_service

ddccontrol_service_SOURCES = interface.h interface.c service.c
ddccontrol_service_LDFLAGS = $(LIBXML2_LDFLAGS) $(LIBINTL)

BUILT_SOURCES = interface.h interface.c
noinst_HEADERS = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = ddccontrol.DDCControl.xml

interface.h interface.c: ddccontrol.DDCControl.xml
	gdbus-codegen --interface-prefix ddccontrol --generate-c-code interface $<
