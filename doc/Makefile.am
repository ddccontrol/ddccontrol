XHTML_STYLESHEET=style.xsl
TIDY_FLAGS = -q -raw -im --output-xhtml yes --doctype transitional --drop-empty-elements no

.NOTPARALLEL:

EXTRA_DIST = main.xml gpl.xml install.xml about.xml report.xml supportedgc.xml thanks.xml techdocs.xml usage.xml style.xsl style-fo.xsl

if BUILD_DOC
html_DATA = html/*

html/*: main.xml gpl.xml install.xml about.xml report.xml supportedgc.xml thanks.xml techdocs.xml usage.xml style.xsl
	rm -rf html.old
	true `mv -f html html.old`
	$(MKDIR_P) html
	$(XSLTPROC) $(XSLTPROC_FLAGS) -o html/ $(top_srcdir)/doc/$(XHTML_STYLESHEET) $<
	@if test "x$(TIDY)" != "xno"; then \
		$(TIDY) $(TIDY_FLAGS) html/*.html || \
			echo "tidy failed; leaving generated HTML unchanged"; \
	else \
		echo "tidy not found; skipping HTML cleanup"; \
	fi

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
	@if test "x$(ASPELL)" = "xno"; then \
		echo "aspell not found; cannot run spell target" >&2; \
		exit 1; \
	fi
	$(ASPELL) --mode=sgml check main.xml

clean-local:
	rm -rf html

MOSTLYCLEANFILES = ddccontrol.fo ddccontrol.pdf

ddccontrol.fo: main.xml gpl.xml style-fo.xsl
	$(XSLTPROC)  \
		$(XSLTPROC_FLAGS) \
		--output  ddccontrol.fo  \
		--stringparam  paper.type  A4 \
		style-fo.xsl  \
		$<

ddccontrol.pdf: ddccontrol.fo
	@if test "x$(FOP)" = "xno"; then \
		echo "fop not found; cannot build PDF documentation" >&2; \
		exit 1; \
	fi
	$(FOP)  -fo  ddccontrol.fo  -pdf  ddccontrol.pdf

pdf: ddccontrol.pdf

endif
