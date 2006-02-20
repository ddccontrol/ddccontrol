localedir = $(datadir)/locale
INCLUDES = -I.. -DLOCALEDIR=\"$(localedir)\"

pkginclude_HEADERS = conf.h monitor_db.h ddcci.h ddcpci-ipc.h


lib_LTLIBRARIES = libddccontrol.la

libddccontrol_la_SOURCES = ddcci.c ddcci.h monitor_db.c monitor_db.h \
   ddcpci-ipc.h i2c-dev.h conf.c conf.h
libddccontrol_la_LIBADD = $(LIBXML2_LDFLAGS)
AM_CFLAGS = $(LIBXML2_CFLAGS)
