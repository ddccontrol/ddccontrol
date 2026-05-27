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
   ddcpci-ipc.h i2c-dev.h conf.c conf.h amd_adl.c amd_adl.h internal.h \
   monitor_db_internal.h

libddccontrol_la_LIBADD = $(LIBXML2_LIBS)

check_PROGRAMS = test_issueurl test_parse_caps test_edid
TESTS = $(check_PROGRAMS)

test_issueurl_SOURCES = tests/test_issueurl.c issueurl.c urlencode.c
test_issueurl_LDADD = libddccontrol.la $(LIBXML2_LIBS)

test_parse_caps_SOURCES = tests/test_parse_caps.c
test_parse_caps_LDADD = libddccontrol.la $(LIBXML2_LIBS)

test_edid_SOURCES = tests/test_edid.c
test_edid_LDADD = libddccontrol.la $(LIBXML2_LIBS)
