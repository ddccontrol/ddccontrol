datadir=@datadir@/ddccontrol-db

nobase_dist_data_DATA = *.xml */*.xml

options.xml.h: options.xml
	perl ../xml2h.pl options.xml

all: options.xml.h

