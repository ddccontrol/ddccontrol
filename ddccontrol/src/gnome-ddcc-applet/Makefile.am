
localedir = $(datadir)/locale
INCLUDES = -I../lib -I.. -DLOCALEDIR=\"$(localedir)\"

DDCC_LDADD = ../lib/libddccontrol.la


pkglib_PROGRAMS = ddcc-applet
ddcc_applet_SOURCES = ddcc-applet.c

ddcc_applet_LDADD = $(GNOME_LIBS) $(DDCC_LDADD)
AM_CFLAGS = $(GNOME_CFLAGS) '-DPKGDATADIR="$(pkgdatadir)"'

pkgdata_DATA = GNOME_ddcc-applet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_ddcc-applet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@PKGLIBDIR\@|$(pkglibdir)|" $< > $@

CLEANFILES = $(server_in_files) $(server_DATA)

@INTLTOOL_SERVER_RULE@

