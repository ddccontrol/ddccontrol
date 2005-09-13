localedir = $(datadir)/locale
INCLUDES = -I.. -DLOCALEDIR=\"$(localedir)\"

pkginclude_HEADERS = conf.h monitor_db.h ddcci.h ddcpci-ipc.h


lib_LTLIBRARIES = libddccontrol.la

libddccontrol_la_SOURCES = ddcci.c ddcci.h monitor_db.c monitor_db.h \
   ddcpci-ipc.h i2c-dev.h conf.c conf.h
AM_CFLAGS = $(LIBXML2_CFLAGS)
