XSLTPROC=@XSLTPROC@
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
XHTML_STYLESHEET=style.xsl

htmldir = $(prefix)/share/doc/@PACKAGE@
dist_html_DATA = html/*
EXTRA_DIST = main.xml gpl.xml style.xsl

html/*: main.xml gpl.xml style.xsl
	rm -rf html.old
	true `mv -f html html.old`
	mkdir html
	cd html; $(XSLTPROC) $(XSLTPROC_FLAGS) ../$(XHTML_STYLESHEET) ../$<
	-tidy -q -raw -im html/*.html

publish: all
	ssh $(SFUSER)@ssh.sf.net "cd /home/groups/d/dd/ddccontrol/htdocs/doc/; rm -rf latest"
	scp -C -r html $(SFUSER)@ssh.sf.net:/home/groups/d/dd/ddccontrol/htdocs/doc/latest

publish-release: all pdf
	ssh $(SFUSER)@ssh.sf.net "cd /home/groups/d/dd/ddccontrol/htdocs/doc/; rm -rf @VERSION@"
	scp -C -r html $(SFUSER)@ssh.sf.net:/home/groups/d/dd/ddccontrol/htdocs/doc/@VERSION@
	scp -C ddccontrol.pdf $(SFUSER)@ssh.sf.net:/home/groups/d/dd/ddccontrol/htdocs/doc/ddccontrol-@VERSION@.pdf

spell:
	aspell --mode=sgml check main.xml

clean:
	rm -rf html

ddccontrol.fo: main.xml gpl.xml style-fo.xsl
	$(XSLTPROC)  \
		--output  ddccontrol.fo  \
		--stringparam  paper.type  A4 \
		style-fo.xsl  \
		$<

ddccontrol.pdf: ddccontrol.fo
	fop  -fo  ddccontrol.fo  -pdf  ddccontrol.pdf

pdf: ddccontrol.pdf
	
