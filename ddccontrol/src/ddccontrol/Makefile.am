localedir = $(datadir)/locale
INCLUDES = -I../lib -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libddcci.a

bin_PROGRAMS = ddccontrol
ddccontrol_SOURCES = main.c
ddccontrol_LDFLAGS = $(LIBXML2_LDFLAGS)
AM_CFLAGS = $(LIBXML2_CFLAGS)
