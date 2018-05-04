<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!--
  Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
-->

<xsl:output method="xml"/>
<xsl:strip-space elements="title"/>

<xsl:param name="asio.version">X.Y.Z</xsl:param>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="blockquote/para">
  <xsl:apply-templates select="para"/>
</xsl:template>

<xsl:template match="title/link">
  <xsl:value-of select="normalize-space(.)"/>
</xsl:template>

<xsl:template match="bridgehead[count(link) &gt; 0]">
  <xsl:element name="bridgehead">
    <xsl:attribute name="renderas">
      <xsl:value-of select="@renderas"/>
    </xsl:attribute>
    <xsl:value-of select="normalize-space(link)"/>
  </xsl:element>
</xsl:template>

<xsl:template match="chapter">
  <article id="asio" name="Asio Reference Manual" dirname="asio">
    <xsl:apply-templates select="chapterinfo"/>
    <title>Asio Reference Manual</title>
    <xsl:apply-templates select="section"/>
  </article>
</xsl:template>

<xsl:template match="chapterinfo">
  <articleinfo>
    <xsl:apply-templates select="*"/>
    <subtitle><xsl:value-of select="$asio.version"/></subtitle>
  </articleinfo>
</xsl:template>

<xsl:template match="section[@id='asio.reference']/informaltable">
  <xsl:element name="informaltable">
    <xsl:attribute name="role">scriptsize</xsl:attribute>
    <xsl:apply-templates select="*"/>
  </xsl:element>
</xsl:template>

<xsl:template match="section[@id='asio.examples']/*/listitem">
  <xsl:element name="listitem">
    <xsl:value-of select="substring-after(normalize-space(ulink), '../')"/>
  </xsl:element>
</xsl:template>

<xsl:template match="imagedata">
  <xsl:element name="imagedata">
    <xsl:attribute name="fileref">
      <xsl:value-of select="@fileref"/>
    </xsl:attribute>
    <xsl:attribute name="scale">75</xsl:attribute>
    <xsl:attribute name="align">center</xsl:attribute>
  </xsl:element>
</xsl:template>

<xsl:template match="programlisting/phrase">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="programlisting/link">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="programlisting/emphasis">
  <xsl:if test="not(contains(., 'more...'))">
    <emphasis><xsl:apply-templates/></emphasis>
  </xsl:if>
</xsl:template>

<xsl:template match="section[@id='asio.index']"></xsl:template>

</xsl:stylesheet>

