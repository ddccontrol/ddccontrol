INCLUDES = -I../lib  -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = ddcpci
ddcpci_LDFLAGS = -Wl,-z,now
ddcpci_SOURCES = main.c nvidia.c i2c-algo-bit.c intel855.c ddcpci.h i2c-algo-bit.h

install-exec-hook:
	chmod +s ${bindir}/ddcpci