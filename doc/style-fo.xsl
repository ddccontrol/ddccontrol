<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>

<xsl:template match="sgmltag[@class='attvalue']">
  <xsl:call-template name="inline.monoseq">
    <xsl:with-param name="content">
      <xsl:call-template name="gentext.startquote"/>
      <xsl:apply-templates/>
      <xsl:call-template name="gentext.endquote"/>
    </xsl:with-param>
  </xsl:call-template>
</xsl:template>

<xsl:attribute-set name="monospace.verbatim.properties" use-attribute-sets="verbatim.properties monospace.properties">
  <xsl:attribute name="text-align">start</xsl:attribute>
  <xsl:attribute name="wrap-option">wrap</xsl:attribute>
  <xsl:attribute name="font-size">
    <xsl:value-of select="$body.font.master * 0.8"></xsl:value-of>
    <xsl:text>pt</xsl:text>
  </xsl:attribute>
  <xsl:attribute name="hyphenate">false</xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="verbatim.properties">
  <xsl:attribute name="text-align">start</xsl:attribute>
  <xsl:attribute name="space-before.minimum">0.8em</xsl:attribute>
  <xsl:attribute name="space-before.optimum">1em</xsl:attribute>
  <xsl:attribute name="space-before.maximum">1.2em</xsl:attribute>
  <xsl:attribute name="space-after.minimum">0.8em</xsl:attribute>
  <xsl:attribute name="space-after.optimum">1em</xsl:attribute>
  <xsl:attribute name="space-after.maximum">1.2em</xsl:attribute>
  <xsl:attribute name="hyphenate">false</xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="monospace.properties">
  <xsl:attribute name="font-family">
    <xsl:value-of select="$monospace.font.family"></xsl:value-of>
  </xsl:attribute>
  <xsl:attribute name="hyphenate">false</xsl:attribute>
  <xsl:attribute name="keep-together">always</xsl:attribute>
</xsl:attribute-set>

</xsl:stylesheet>
