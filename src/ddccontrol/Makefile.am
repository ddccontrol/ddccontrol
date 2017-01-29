localedir = $(datadir)/locale
INCLUDES = -I@top_srcdir@/src/lib -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddccontrol.la

bin_PROGRAMS = ddccontrol
ddccontrol_SOURCES = main.c
ddccontrol_LDFLAGS = $(LIBXML2_LDFLAGS) $(LIBINTL)
AM_CFLAGS = $(LIBXML2_CFLAGS)
