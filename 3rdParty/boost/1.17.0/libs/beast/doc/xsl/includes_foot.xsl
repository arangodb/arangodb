<!-- INCLUDES_FOOT_TEMPLATE BEGIN -->
  <xsl:choose>
    <xsl:when test="contains($file, 'boost/beast/core')">
      <xsl:text>&#xd;&#xd;Convenience header [include_file boost/beast/core.hpp]&#xd;</xsl:text>
    </xsl:when>
    <xsl:when test="contains($file, 'boost/beast/http')">
      <xsl:text>&#xd;&#xd;Convenience header [include_file boost/beast/http.hpp]&#xd;</xsl:text>
    </xsl:when>
    <xsl:when test="contains($file, 'boost/beast/ssl')">
      <xsl:text>&#xd;&#xd;Convenience header [include_file boost/beast/ssl.hpp]&#xd;</xsl:text>
    </xsl:when>
    <xsl:when test="contains($file, 'boost/beast/websocket')">
      <xsl:text>&#xd;&#xd;Convenience header [include_file boost/beast/websocket.hpp]&#xd;</xsl:text>
    </xsl:when>
    <xsl:when test="contains($file, 'boost/beast/zlib')">
      <xsl:text>&#xd;&#xd;Convenience header [include_file boost/beast/zlib.hpp]&#xd;</xsl:text>
    </xsl:when>
  </xsl:choose>
<!-- INCLUDES_FOOT_TEMPLATE END -->
