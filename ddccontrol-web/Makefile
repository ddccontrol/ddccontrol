STYLEDIR=http://docbook.sourceforge.net/release/website/2.4.1/xsl

all: custom-tabular.xsl
	make website

include  depends.tabular

autolayout.xml: layout.xml Makefile
	xsltproc  --output  $@ \
	   $(STYLEDIR)/autolayout.xsl  $<  
	make  depends

depends: autolayout.xml Makefile
	xsltproc  \
	   --output  depends.tabular \
	   --stringparam  output-root  htdocs  \
	   $(STYLEDIR)/makefile-dep.xsl  \
	   $<

website.database.xml:  autolayout.xml Makefile
	xsltproc \
	   --output $@ \
	   $(STYLEDIR)/website-targets.xsl  $<

%.html: autolayout.xml custom-tabular.xsl Makefile
	xsltproc \
	   --output  $@ \
	   --stringparam autolayout-file autolayout.xml \
	   --stringparam  website.database.document website.database.xml \
	   --stringparam  output-root  htdocs  \
	   --stringparam  suppress.homepage.title  '0'  \
	   --stringparam  navbgcolor \#60A0FF \
	   --stringparam  collect.xref.targets yes \
	   custom-tabular.xsl \
	   $(filter-out autolayout.xml custom-tabular.xsl Makefile,$^)

.PHONY : clean

update:
	ssh $(SFUSER)@ssh.sf.net "cd /home/groups/d/dd/ddccontrol/; cvs update -d"

spell:
	aspell --mode=sgml check website.xml
	aspell --mode=sgml check doc.xml
	aspell --mode=sgml check download.xml
	aspell --mode=sgml check layout.xml
