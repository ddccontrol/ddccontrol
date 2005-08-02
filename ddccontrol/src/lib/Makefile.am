localedir = $(datadir)/locale
INCLUDES = -I.. -DLOCALEDIR=\"$(localedir)\"


noinst_LIBRARIES = libddcci.a

libddcci_a_SOURCES = ddcci.c ddcci.h monitor_db.c monitor_db.h \
   ddcpci-ipc.h i2c-dev.h profile.c profile.h
AM_CFLAGS = $(LIBXML2_CFLAGS)