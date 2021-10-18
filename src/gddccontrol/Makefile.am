localedir = $(datadir)/locale
AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_srcdir)/src -DLOCALEDIR=\"$(localedir)\"

EXTRA_DIST = gddccontrol.desktop.in gddccontrol.png gddccontrol-bluecurve.png

bin_PROGRAMS = gddccontrol
gddccontrol_SOURCES = main.c notebook.c notebook.h gprofile.c fspatterns.c
gddccontrol_LDFLAGS = $(GNOME_LDFLAGS) -lm
gddccontrol_LDADD = ../daemon/libddccontrol_dbus_client.la ../lib/libddccontrol.la
AM_CFLAGS = $(GNOME_CFLAGS)


desktopdir = $(datadir)/applications
desktop_in_files = gddccontrol.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

MOSTLYCLEANFILES = $(desktop_DATA)

icondir = $(datadir)/icons/hicolor/48x48/apps
icon_DATA = gddccontrol.png

Bluecurvedir = $(datadir)/icons/Bluecurve/48x48/apps
Bluecurve_DATA = gddccontrol.png

#
# GTK icon cache
#

gtk_update_icon_cache = \
	gtk-update-icon-cache -f -t $(datadir)/icons/hicolor; \
	gtk-update-icon-cache -f -t $(datadir)/icons/Bluecurve

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache

update-icon-cache:
	if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	else \
	        echo "*** Icon cache not updated.  After (un)install, run this:"; \
	        echo "***   $(gtk_update_icon_cache)"; \
	        fi
