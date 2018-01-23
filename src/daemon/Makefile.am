bin_PROGRAMS = service client

client_SOURCES = interface.h interface.c client.c
service_SOURCES = interface.h interface.c service.c

BUILT_SOURCES = interface.h interface.c
noinst_HEADERS = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = ddccontrol.DDCControl.xml

interface.h interface.c: ddccontrol.DDCControl.xml
	gdbus-codegen --interface-prefix ddccontrol --generate-c-code interface $<
