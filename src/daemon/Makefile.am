SUBDIRS = data

AM_CFLAGS = $(LIBXML2_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src/lib

# install D-Bus API specification for ddccontrol.DDCControl.xml interface
dbusinterfacesdir = $(datadir)/dbus-1/interfaces
dist_dbusinterfaces_DATA = ddccontrol.DDCControl.xml

# install to pkglibexec, because it's not intended to be run by user
pkglibexec_PROGRAMS = ddccontrol_service

ddccontrol_service_SOURCES = service.c
ddccontrol_service_LDFLAGS = $(LIBXML2_LIBS) $(LIBINTL)
ddccontrol_service_LDADD   = libdbusinterface.la ../lib/libddccontrol.la

# D-Bus client library
lib_LTLIBRARIES = libddccontrol_dbus_client.la

libddccontrol_dbus_client_la_SOURCES = dbus_client.c
libddccontrol_dbus_client_la_LDFLAGS = $(LIBXML2_LIBS)
libddccontrol_dbus_client_la_LIBADD   = libdbusinterface.la ../lib/libddccontrol.la

# interface library
noinst_LTLIBRARIES = libdbusinterface.la
libdbusinterface_la_SOURCES = interface.c interface.h

BUILT_SOURCES = interface.h interface.c
noinst_HEADERS = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = ddccontrol.DDCControl.xml

interface.h interface.c: ddccontrol.DDCControl.xml
	gdbus-codegen --interface-prefix ddccontrol --generate-c-code interface $<
