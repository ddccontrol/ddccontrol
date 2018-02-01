
# D-Bus config file
dbusconfdir = $(sysconfdir)/dbus-1/system.d
dist_dbusconf_DATA = ddccontrol.DDCControl.conf


# D-Bus service file
dbusservicedir       = $(datadir)/dbus-1/system-services
dbusservice_in_files = ddccontrol.DDCControl.service.in
dbusservice_DATA     = $(dbusservice_in_files:.service.in=.service)

$(dbusservice_DATA): $(dbusservice_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(pkglibexecdir)|" $< > $@

EXTRA_DIST     = $(dbusservice_in_files)
DISTCLEANFILES = $(dbusservice_DATA)


# systemd service file
if HAVE_SYSTEMD
systemdservicedir       = $(systemdsystemunitdir)
systemdservice_in_files = ddccontrol.service.in
systemdservice_DATA     = $(systemdservice_in_files:.service.in=.service)
$(systemdservice_DATA): $(systemdservice_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(pkglibexecdir)|" $< > $@
	
EXTRA_DIST     += $(systemdservice_in_files)
DISTCLEANFILES += $(systemdservice_DATA)
endif
