INCLUDES = -I../lib

bin_PROGRAMS = ddcpci
ddcpci_SOURCES = main.c nvidia.c i2c-algo-bit.c intel855.c

install-exec-hook:
	chmod +s ${bindir}/ddcpci