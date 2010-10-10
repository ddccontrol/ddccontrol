INCLUDES = -I@top_srcdir@/src -I@top_srcdir@/src/lib  -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = ddcpci
ddcpci_LDFLAGS = -Wl,-z,now
ddcpci_SOURCES = main.c nvidia.c radeon.c i2c-algo-bit.c intel810.c ddcpci.h i2c-algo-bit.h via.c sis.c

EXTRA_DIST = intel740.c

install-exec-hook:
	chmod 4711 ${DESTDIR}/${bindir}/ddcpci
