<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:html='http://www.w3.org/1999/xhtml'
                xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
                xmlns:str="http://exslt.org/strings"
                extension-element-prefixes="str"
                exclude-result-prefixes="html doc str"
                version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/website/2.4.1/xsl/tabular.xsl"/>

<!-- Replace the text in these templates with whatever you want -->
<!-- to appear in the respective location on the home page. -->

<xsl:template name="home.navhead">
</xsl:template>

<xsl:template name="home.navhead.upperright">
</xsl:template>

<xsl:template name="home.navhead.separator">
</xsl:template>

<xsl:template match="toc">
  <xsl:param name="pageid" select="@id"/>

  <xsl:variable name="relpath">
    <xsl:call-template name="toc-rel-path">
      <xsl:with-param name="pageid" select="$pageid"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="banner"
                select="/autolayout/config[@param='bannertext-tabular'][1]"/>

  <xsl:choose>
    <xsl:when test="$pageid = @id">
      <h2 id="title"><xsl:value-of select="$banner/@value"/></h2>
    </xsl:when>
    <xsl:otherwise>
      <a class="title" href="{$relpath}{@dir}{$filename-prefix}{@filename}">
        <h2 id="title"><xsl:value-of select="$banner/@value"/></h2>
      </a>
    </xsl:otherwise>
  </xsl:choose>
  <br clear="all"/>
  <br/>

  <xsl:apply-templates select="tocentry">
    <xsl:with-param name="pageid" select="$pageid"/>
    <xsl:with-param name="relpath" select="$relpath"/>
  </xsl:apply-templates>
  <br/>
</xsl:template>

<xsl:template name="webpage.footer"/>

<xsl:template name="webpage.table.footer">
  <xsl:variable name="page" select="."/>
  <xsl:variable name="id" select="$page/@id"/>
  <xsl:variable name="cvsid1" select="substring-before($page/config[@param='cvsid']/@value, ' $')"/>
  <xsl:variable name="cvsid" select="substring-after($cvsid1, '$Id: ')"/>

  <xsl:variable name="title">
    <xsl:value-of select="$page/head/title[1]"/>
  </xsl:variable>
  <xsl:variable name="footers" select="$page/config[@param='footer']
                                       |$page/config[@param='footlink']"/>

  <tr>
    <td width="{$navtocwidth}" bgcolor="{$navbgcolor}">&#160;</td>
    <td><hr/></td>
  </tr>

  <tr>
    <td width="{$navtocwidth}" bgcolor="{$navbgcolor}"
        valign="top" align="center">
        <p><A href="http://sourceforge.net">
<IMG src="http://sourceforge.net/sflogo.php?group_id=117933&amp;type=2" 
  width="125" height="37" border="0" alt="SourceForge.net Logo"></IMG></A>
          </p>
          <p>
      <span class="footdate">
        <!-- cvsid = "website.xml,v 1.1 2005/03/25 12:08:10 nboichat Exp" -->
        <!-- timeString = "dow mon dd hh:mm:ss TZN yyyy" -->

        <xsl:text>Last updated: </xsl:text>
	<br/>
	<xsl:value-of select="str:tokenize($cvsid,' ')[3]"/>
        <xsl:text> </xsl:text>
	<xsl:value-of select="str:tokenize($cvsid,' ')[4]"/>
        by <xsl:value-of select="str:tokenize($cvsid,' ')[5]"/>
        <br/>
        CVS info:
        <br/>
		<xsl:value-of select="str:tokenize($cvsid,' ')[1]"/>
        <xsl:text> </xsl:text>
		<xsl:value-of select="str:tokenize($cvsid,' ')[2]"/>
	<xsl:text> </xsl:text>
		<xsl:value-of select="str:tokenize($cvsid,' ')[6]"/>
	<xsl:text> </xsl:text>
		<xsl:value-of select="str:tokenize($cvsid,' ')[7]"/>
        <br/>
      </span>
      </p>
    </td>

    <td valign="top" align="center">
      <xsl:variable name="tocentry" select="$autolayout//*[@id=$id]"/>
      <xsl:variable name="toc"
                    select="($tocentry/ancestor-or-self::toc[1]
                            | $autolayout//toc[1])[last()]"/>

      <xsl:choose>
        <xsl:when test="not($toc)">
          <xsl:message>
            <xsl:text>Cannot determine TOC for </xsl:text>
            <xsl:value-of select="$id"/>
          </xsl:message>
        </xsl:when>
        <xsl:when test="$toc/@id = $id">
          <!-- nop; this is the home page -->
        </xsl:when>
        <xsl:otherwise>
          <span class="foothome">
            <a>
              <xsl:attribute name="href">
                <xsl:call-template name="homeuri"/>
              </xsl:attribute>
              <xsl:text>Home</xsl:text>
            </a>
            <xsl:if test="$footers">
              <xsl:text> | </xsl:text>
            </xsl:if>
          </span>
        </xsl:otherwise>
      </xsl:choose>

      <xsl:apply-templates select="$footers" mode="footer.link.mode"/>
    </td>
  </tr>
</xsl:template>

</xsl:stylesheet>
