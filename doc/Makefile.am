XHTML_STYLESHEET=style.xsl

dist_html_DATA = html/*
EXTRA_DIST = main.xml gpl.xml install.xml about.xml report.xml supportedgc.xml thanks.xml techdocs.xml usage.xml style.xsl style-fo.xsl

if BUILD_DOC
html/*: main.xml gpl.xml install.xml about.xml report.xml supportedgc.xml thanks.xml techdocs.xml usage.xml style.xsl
	rm -rf html.old
	true `mv -f html html.old`
	$(MKDIR_P) html
	$(XSLTPROC) $(XSLTPROC_FLAGS) -o html/ $(top_srcdir)/doc/$(XHTML_STYLESHEET) $<
	-$(TIDY) -q -raw -im html/*.html

publish: all
	rm -rf $(abs_top_builddir)/docs/doc/latest
	$(MKDIR_P) $(abs_top_builddir)/docs/doc
	cp -R html $(abs_top_builddir)/docs/doc/latest

publish-release: all pdf
	rm -rf $(abs_top_builddir)/docs/doc/$(VERSION)
	$(MKDIR_P) $(abs_top_builddir)/docs/doc
	cp -R html $(abs_top_builddir)/docs/doc/$(VERSION)
	cp ddccontrol.pdf $(abs_top_builddir)/docs/doc/ddccontrol-$(VERSION).pdf

spell:
	$(ASPELL) --mode=sgml check main.xml

clean-local:
	rm -rf html

MOSTLYCLEANFILES = ddccontrol.fo ddccontrol.pdf

ddccontrol.fo: main.xml gpl.xml style-fo.xsl
	$(XSLTPROC)  \
		--output  ddccontrol.fo  \
		--stringparam  paper.type  A4 \
		style-fo.xsl  \
		$<

ddccontrol.pdf: ddccontrol.fo
	$(FOP)  -fo  ddccontrol.fo  -pdf  ddccontrol.pdf

pdf: ddccontrol.pdf

endif
