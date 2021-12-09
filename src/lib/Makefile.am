localedir = $(datadir)/locale
AM_CFLAGS = -DDDCPCIDIR=\"${pkglibexecdir}\" $(LIBXML2_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DLOCALEDIR=\"$(localedir)\"

pkginclude_HEADERS = conf.h monitor_db.h ddcci.h ddcpci-ipc.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ddccontrol.pc

$(pkgconfig_DATA): $(srcdir)/ddccontrol.pc.in $(top_builddir)/config.status

modulesdir = $(libdir)/modules-load.d/
modules_DATA = ddccontrol-i2c-dev.conf

lib_LTLIBRARIES = libddccontrol.la

libddccontrol_la_SOURCES = ddcci.c ddcci.h monitor_db.c monitor_db.h \
   ddcpci-ipc.h i2c-dev.h conf.c conf.h amd_adl.c amd_adl.h internal.h

libddccontrol_la_LIBADD = $(LIBXML2_LIBS)
