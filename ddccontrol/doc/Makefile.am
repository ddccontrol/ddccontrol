XSLTPROC=@XSLTPROC@
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
XHTML_STYLESHEET=style.xsl

htmldir = $(prefix)/share/doc/@PACKAGE@
dist_html_DATA = html/*

html/*: main.xml gpl.xml style.xsl
	rm -rf html.old
	true `mv -f html html.old`
	mkdir html
	cd html; $(XSLTPROC) $(XSLTPROC_FLAGS) ../$(XHTML_STYLESHEET) ../$<

publish: all
	ssh $(SFUSER)@ssh.sf.net "cd /home/groups/d/dd/ddccontrol/htdocs/doc/; rm -rf latest"
	scp -C -r html $(SFUSER)@ssh.sf.net:/home/groups/d/dd/ddccontrol/htdocs/doc/latest

spell:
	aspell --mode=sgml check main.xml

clean:
	rm -rf html
