<?xml version="1.0"?>

<!-- Copyright (C) 2016 and later: Unicode, Inc. and others. -->
<!-- License & terms of use: http://www.unicode.org/copyright.html -->

<!-- Copyright (C) 2011 IBM Corporation and Others. All Rights Reserved. -->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  
  <xsl:template match="/perfTestResults">
    <report category="test">
      <!-- For now, include ICU type and version in a comment. -->
      <xsl:comment>icu4<xsl:value-of select="@icu"/>-<xsl:value-of select="@version"/></xsl:comment>
      
      <xsl:for-each select="perfTestResult">
        <xsl:element name="test">
          <xsl:attribute name="duration">
            <xsl:value-of select="@time"/>
          </xsl:attribute>
          <xsl:attribute name="status">success</xsl:attribute>
          <xsl:attribute name="name">
            <xsl:value-of select="@test"/>
          </xsl:attribute>
          <xsl:attribute name="fixture">
          </xsl:attribute>
          <xsl:attribute name="file">source/test/perf</xsl:attribute>
          <xsl:attribute name="stdout">iterations: <xsl:value-of select="@iterations"/></xsl:attribute>
        </xsl:element>
      </xsl:for-each>
    </report>
  </xsl:template>
</xsl:stylesheet>
