<?xml version='1.0'?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
 version="1.0"
>

<!-- ************** FOP ************** -->
<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>
<xsl:param name="fop1.extensions">1</xsl:param>
<xsl:param name="sidebar.float.type">none</xsl:param>
<xsl:param name="alignment">left</xsl:param>

<!--
Make all hyperlinks blue colored:
-->
<xsl:attribute-set name="xref.properties">
  <xsl:attribute name="color">blue</xsl:attribute>
</xsl:attribute-set>

<!--
Put a box around admonishments and keep them together:
-->
<xsl:attribute-set name="graphical.admonition.properties">
  <xsl:attribute name="border-color">#FF8080</xsl:attribute>
  <xsl:attribute name="border-width">1px</xsl:attribute>
  <xsl:attribute name="border-style">solid</xsl:attribute>
  <xsl:attribute name="padding-left">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-right">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-top">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-bottom">0.2cm</xsl:attribute>
  <xsl:attribute name="keep-together.within-page">1</xsl:attribute>
  <xsl:attribute name="margin-left">0pt</xsl:attribute>
  <xsl:attribute name="margin-right">0pt</xsl:attribute>
</xsl:attribute-set>

<!--
Put a box around code blocks, also set the font size
and keep the block together if we can using the widows 
and orphans controls.  Hyphenation and line wrapping
is also turned on, so that long lines of code don't
bleed off the edge of the page, a carriage return
symbol is used as the hyphenation character:
-->
<xsl:attribute-set name="monospace.verbatim.properties">
  <xsl:attribute name="border-color">#DCDCDC</xsl:attribute>
  <xsl:attribute name="border-width">1px</xsl:attribute>
  <xsl:attribute name="border-style">solid</xsl:attribute>
  <xsl:attribute name="padding-left">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-right">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-top">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-bottom">0.2cm</xsl:attribute>
  <xsl:attribute name="widows">6</xsl:attribute>
  <xsl:attribute name="orphans">40</xsl:attribute>
  <xsl:attribute name="font-size">9pt</xsl:attribute>
  <xsl:attribute name="margin-left">0pt</xsl:attribute>
  <xsl:attribute name="background-color">#EEEEEE</xsl:attribute>
  <xsl:attribute name="margin-right">0pt</xsl:attribute>
  <!--
  <xsl:attribute name="hyphenate">true</xsl:attribute>
  <xsl:attribute name="wrap-option">wrap</xsl:attribute>
  <xsl:attribute name="hyphenation-character">&#x21B5;</xsl:attribute>
  -->
  <xsl:attribute name="hyphenate">false</xsl:attribute>
  <xsl:attribute name="wrap-option">no-wrap</xsl:attribute>
</xsl:attribute-set>

<!--Regular monospace text should have the same font size as code blocks etc-->
<xsl:attribute-set name="monospace.properties">
  <xsl:attribute name="font-size">9pt</xsl:attribute>
</xsl:attribute-set>
  
<!-- 
Put some small amount of padding around table cells, and keep tables
together on one page if possible:
-->
<xsl:attribute-set name="table.cell.padding">
  <xsl:attribute name="padding-left">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-right">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-top">0.2cm</xsl:attribute>
  <xsl:attribute name="padding-bottom">0.2cm</xsl:attribute>
</xsl:attribute-set>

<!--Formal and informal tables have the same properties-->
<xsl:param name="table.frame.border.thickness">medium</xsl:param>
<xsl:param name="table.cell.border.thickness">medium</xsl:param>
<xsl:param name="table.frame.border.color">#DCDCDC</xsl:param>
<xsl:param name="table.cell.border.color">#DCDCDC</xsl:param>

<!--Formal and informal tables have the same properties
Using widow-and-orphan control here gives much better
results for very large tables than a simple "keep-together"
instruction
-->
<xsl:attribute-set name="table.properties">
  <xsl:attribute name="keep-together.within-page">1</xsl:attribute>
</xsl:attribute-set>
<xsl:attribute-set name="informaltable.properties">
  <xsl:attribute name="keep-together.within-page">1</xsl:attribute>
</xsl:attribute-set>

<!--
General default options go here:
* Borders are mid-grey.
* Body text is not indented compared to the titles.
* Page margins are a rather small 0.5in, but we need
  all the space we can get for code blocks.
* Paper size is A4: an ISO standard, slightly taller and narrower than US Letter.
* Use SVG graphics for admonishments: the bitmaps look awful in PDF's.
* Disable draft mode so we're not constantly trying to download the necessary graphic.
* Set default image paths to pull down direct from SVN: individual Jamfiles can override this
  and pass an absolute path to local versions of the images, but we can't get that here, so
  we'll use SVN instead so that most things "just work".
-->
<xsl:param name="body.start.indent">0pt</xsl:param>
<xsl:param name="admon.graphics">1</xsl:param>
<xsl:param name="admon.graphics.extension">.svg</xsl:param>
<xsl:param name="draft.mode">no</xsl:param>
<xsl:param name="double.sided">1</xsl:param>
<xsl:param name="show.bookmarks" select="1"></xsl:param>
<xsl:param name="page.margin.inner">0.75in</xsl:param>
<xsl:param name="page.margin.outer">0.50in</xsl:param>
<xsl:param name="body.margin.top">0.50in</xsl:param>
<xsl:param name="section.autolabel" select="1"></xsl:param>
<xsl:param name="section.autolabel.max.depth">2</xsl:param>
<xsl:param name="ulink.show" select="0"></xsl:param>
<xsl:param name="admon.graphics.path">http://svn.boost.org/svn/boost/trunk/doc/src/images/</xsl:param>
<xsl:param name="callout.graphics.path">http://svn.boost.org/svn/boost/trunk/doc/src/images/callouts/</xsl:param>

<!-- ******* Table of Contents ******** -->
<!-- How far down sections get TOC's -->
<xsl:param name = "toc.section.depth" select="3" />
<!-- Max depth in each TOC: -->
<xsl:param name = "toc.max.depth" select="3" />
<!-- How far down we go with TOC's -->
<xsl:param name="generate.section.toc.level" select="0" />

<!-- ******* Chapter Titles ******* -->
<xsl:param name="force.blank.pages" select="1"></xsl:param>

<!-- 
<l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
  <l:l10n language="en"> 
    <l:context name="title-numbered">
      <l:template name="chapter" text="%n.&#160;%t"/> 
    </l:context>    
  </l:l10n>
</l:i18n>
-->

<xsl:attribute-set name="chapter.title.properties">
  <xsl:attribute name="font-family">
    <xsl:value-of select="$title.font.family"/>
  </xsl:attribute>
  <xsl:attribute name="font-weight">bold</xsl:attribute>
  <!-- font size is added dynamically by section.heading template -->
  <xsl:attribute name="keep-with-next.within-column">always</xsl:attribute>
  <xsl:attribute name="space-before.minimum">0.8em</xsl:attribute>
  <xsl:attribute name="space-before.optimum">1.0em</xsl:attribute>
  <xsl:attribute name="space-before.maximum">1.2em</xsl:attribute>
  <xsl:attribute name="text-align">left</xsl:attribute>
  <xsl:attribute name="start-indent">0pt</xsl:attribute>
</xsl:attribute-set>

<!-- ******* Section Level 5 properties  ******* -->
<!-- used by bridgeheads -->
<xsl:attribute-set name="section.title.level5.properties">
  <xsl:attribute name="font-size">
    <xsl:value-of select="$body.font.master * 1.2"/>
    <xsl:text>pt</xsl:text>
  </xsl:attribute>
</xsl:attribute-set>

</xsl:stylesheet>
