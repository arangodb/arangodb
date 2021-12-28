<xsl:stylesheet version="3.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  exclude-result-prefixes="xs"
  expand-text="yes">

  <xsl:variable name="doc-ref" select="'static_string.ref'"/>
  <xsl:variable name="doc-ns" select="'boost::static_strings'"/>
  <xsl:variable name="include-private-members" select="false()"/>

</xsl:stylesheet>
