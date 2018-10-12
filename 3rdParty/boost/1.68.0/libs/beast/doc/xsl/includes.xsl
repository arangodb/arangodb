<!-- INCLUDES_TEMPLATE BEGIN -->
  <xsl:text>Defined in header [include_file </xsl:text>
  <xsl:value-of select="substring-after($file,'/include/')"/>
  <xsl:text>]&#xd;&#xd;</xsl:text>
<!-- INCLUDES_TEMPLATE END -->
