AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/lib  -DLOCALEDIR=\"$(localedir)\"

# install to libexec, because it's not intended to be run by user
pkglibexec_PROGRAMS = ddcpci

ddcpci_LDFLAGS = -Wl,-z,now
ddcpci_SOURCES = main.c nvidia.c radeon.c i2c-algo-bit.c ddcpci.h i2c-algo-bit.h via.c

if HAVE_SYS_IO
ddcpci_SOURCES += intel810.c sis.c
endif

EXTRA_DIST = intel740.c

install-exec-hook:
	chmod 4711 ${DESTDIR}/${pkglibexecdir}/ddcpci
