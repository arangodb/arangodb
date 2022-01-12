<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- ************** HTML ************** -->
<xsl:import href="http://www.boost.org/tools/boostbook/xsl/html.xsl"/>

<!-- remove "Chapter 1" from first page -->
<xsl:param name="chapter.autolabel" select="0"/>
<!-- leave the html files in the directory ../html -->
<xsl:param name="base.dir" select="'../html/'"/>

<!-- ******* Table of Contents ******** -->
<!-- How far down sections get TOC's -->
<xsl:param name = "toc.section.depth" select="2" />

<!-- Max depth in each TOC: -->
<xsl:param name = "toc.max.depth" select="2" />

<!-- How far down we go with TOC's -->
<xsl:param name="generate.section.toc.level" select="2" />

<!-- ************ Chunking ************ -->

<!--
BoostBook takes a section node id like safe_numeric.safe_cast
and renders it as safe_numeric/safe_cast. Presumably they do this
so they can make a huge "book" with all the libraries in subdirectories.
But we want something different.  To my mind, this should have been
done using the library "directory" attribute.  But of course that
doesn't matter now.  We'll just re-hack the path to eliminate
the "safe_numeric/" from the above example.
-->

<xsl:template match="*" mode="recursive-chunk-filename">
    <xsl:variable name="their">
        <xsl:apply-imports mode="recursive-chunk-filename" select="."/>
    </xsl:variable>
    <xsl:choose>
    <xsl:when test="contains($their, '/')">
        <xsl:value-of select="substring-after($their, '/')" />
    </xsl:when>
    <xsl:otherwise>
        <xsl:value-of select="$their"/>
    </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- don't make first sections special - leave TOC in different file -->
<xsl:param name="chunk.first.sections" select="3" />

<!-- How far down we chunk nested sections -->
<!-- 
Note: each chunk have to start with  a section with an id
Otherwise the chunk (i.e. file) will be lost.  There is no 
checking of this
-->
<xsl:param name="chunk.section.depth" select="2" />

</xsl:stylesheet>
