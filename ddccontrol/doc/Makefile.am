XSLTPROC=@XSLTPROC@
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
XHTML_STYLESHEET=@DOCBOOK_ROOT@/chunk.xsl

htmldir = $(prefix)/share/doc/@PACKAGE@
dist_html_DATA = *.html

*.html: main.xml
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(XHTML_STYLESHEET) $<

clean:
	rm -f *.html
