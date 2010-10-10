
localedir = $(datadir)/locale
INCLUDES = -I@top_srcdir@/src/lib -I@top_srcdir@/src -DLOCALEDIR=\"$(localedir)\"

DDCC_LDADD = ../lib/libddccontrol.la

EXTRA_DIST = GNOME_ddcc-applet.server.in.in GNOME_ddcc-applet.xml

applet_PROGRAMS = ddcc-applet
appletdir = $(libexecdir)

ddcc_applet_SOURCES = ddcc-applet.c ddcc-applet.h
ddcc_applet_LDADD = $(GNOME_LDFLAGS) $(DDCC_LDADD)
AM_CFLAGS = $(GNOME_CFLAGS) '-DPKGDATADIR="$(pkgdatadir)"'

pkgdata_DATA = GNOME_ddcc-applet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_ddcc-applet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in) Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@appletdir\@|$(appletdir)|" \
	    $< > $@

CLEANFILES = $(server_in_files) $(server_DATA)

@INTLTOOL_SERVER_RULE@
