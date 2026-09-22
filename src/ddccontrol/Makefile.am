localedir = $(datadir)/locale
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/lib -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddccontrol.la ../daemon/libddccontrol_dbus_client.la

bin_PROGRAMS = ddccontrol
ddccontrol_SOURCES = main.c printing.c ddccontrol.h \
	../lib/urlencode.c ../lib/urlencode.h \
	../lib/issueurl.c ../lib/issueurl.h
ddccontrol_LDFLAGS = $(LIBXML2_LIBS) $(LIBINTL)
AM_CFLAGS = $(LIBXML2_CFLAGS)

TESTS = tests/test_monitor_file.sh
EXTRA_DIST = $(TESTS)
TESTS_ENVIRONMENT = abs_top_srcdir='$(abs_top_srcdir)' abs_top_builddir='$(abs_top_builddir)'
