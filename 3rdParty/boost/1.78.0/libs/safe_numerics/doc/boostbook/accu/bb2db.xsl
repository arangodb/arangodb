<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:import href="http://www.boost.org/tools/boostbook/xsl/docbook.xsl"/>

  <!-- highlight C++ source code -->
  <xsl:template match="programlisting">
    <xsl:apply-templates select="." mode="annotation"/>
  </xsl:template>

</xsl:stylesheet>
