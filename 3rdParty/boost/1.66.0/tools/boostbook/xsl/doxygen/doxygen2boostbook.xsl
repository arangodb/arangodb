<?xml version="1.0" encoding="utf-8"?>
<!--
   Copyright (c) 2002 Douglas Gregor <doug.gregor -at- gmail.com>
  
   Distributed under the Boost Software License, Version 1.0.
   (See accompanying file LICENSE_1_0.txt or copy at
   http://www.boost.org/LICENSE_1_0.txt)
  -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
  <xsl:import href="../lookup.xsl"/>

  <!-- Set this parameter to a space-separated list of headers that
       will be included in the output (all others are ignored). If this
       parameter is omitted or left as the empty string, all headers will
       be output. -->
  <xsl:param name="boost.doxygen.headers" select="''"/>

  <!-- The common prefix to all headers -->
  <xsl:param name="boost.doxygen.header.prefix" select="'boost'"/>

  <!-- The text that Doxygen places in overloaded functions. Damn them
       for forcing us to compare TEXT just to figure out what's overloaded
       and what isn't. -->
  <xsl:param name="boost.doxygen.overload">
    This is an overloaded member function, provided for convenience. It differs from the above function only in what argument(s) it accepts.
  </xsl:param>

  <!-- The namespace used to identify code that should not be
       processed at all. -->
  <xsl:param name="boost.doxygen.detailns">detail</xsl:param>

  <!-- The substring used to identify unspecified types that we can't
       mask from within Doxygen. This is a hack (big surprise). -->
  <xsl:param name="boost.doxygen.detail"><xsl:value-of select="$boost.doxygen.detailns"/>::</xsl:param>

  <!-- The title that will be used for the BoostBook library reference emitted. 
       If left blank, BoostBook will assign a default title. -->
  <xsl:param name="boost.doxygen.reftitle" select="''"/>

  <!-- The id used for the library-reference. By default, it is the normalized
       form of the reftitle. -->
  <xsl:param name="boost.doxygen.refid" select="''"/>

  <!-- The directory into which png files corresponding to LaTeX formulas will be found. -->
  <xsl:param name="boost.doxygen.formuladir" select="'images/'"/>

  <xsl:output method="xml" indent="no" standalone="yes"/>

  <xsl:key name="compounds-by-kind" match="compounddef" use="@kind"/>
  <xsl:key name="compounds-by-id" match="compounddef" use="@id"/>
  <xsl:key name="members-by-id" match="memberdef" use="@id" />

  <!-- Add trailing slash to formuladir if missing -->

  <xsl:variable name="boost.doxygen.formuladir.fixed">
    <xsl:choose>
      <xsl:when test="substring(boost.doxygen.formuladir, string-length(boost.doxygen.formuladir) - 1) = '/'">
        <xsl:value-of select="$boost.doxygen.formuladir" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="concat($boost.doxygen.formuladir, '/')" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:strip-space elements="briefdescription detaileddescription inbodydescription"/>

  <xsl:template name="kind-error-message">
    <xsl:param name="message"/>

    <xsl:variable name="location" select=".//location[1]" />
    <xsl:variable name="name" select="./name" />

    <xsl:message>
      <xsl:if test="$location">
        <xsl:value-of select="concat($location/@file, ':', $location/@line, ': ')" />
      </xsl:if>
      <xsl:value-of select="concat($message, ' with kind=', @kind)" />
      <xsl:if test="$name">
        <xsl:value-of select="concat(' (name=', $name, ') ')" />
      </xsl:if>
    </xsl:message>
  </xsl:template>

  <!-- translate-name: given a string, return a string suitable for use as a refid -->
  <xsl:template name="translate-name">
    <xsl:param name="name"/>
    <xsl:value-of select="translate($name,
                                    'ABCDEFGHIJKLMNOPQRSTUVWXYZ ~!%^&amp;*()[].,&lt;&gt;|/ +-=',
                                    'abcdefghijklmnopqrstuvwxyz_____________________')"/>
  </xsl:template>

  <xsl:template match="/">
    <xsl:apply-templates select="doxygen"/>
  </xsl:template>

  <xsl:template match="doxygen">
    <library-reference>
      <xsl:if test="string($boost.doxygen.reftitle) != ''">
        <!-- when a reference section has a reftitle, also give it a refid. The id
             is determined by the boost.doxygen.refid param, which defaults to a 
             normalized form of the boost.doxygen.reftitle -->
        <xsl:attribute name="id">
          <xsl:choose>
            <xsl:when test="string($boost.doxygen.refid) != ''">
              <xsl:value-of select="$boost.doxygen.refid"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:call-template name="translate-name">
                <xsl:with-param name="name" select="$boost.doxygen.reftitle"/>
              </xsl:call-template>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:attribute>

        <title><xsl:copy-of select="$boost.doxygen.reftitle"/></title>
      </xsl:if>
      <xsl:apply-templates select="key('compounds-by-kind', 'file')"/>
    </library-reference>
  </xsl:template>

  <xsl:template match="compounddef">
    <!-- The set of innernamespace nodes that limits our search -->
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:choose>
      <!-- If the string INTERNAL ONLY is in the description, don't
           emit this entity. This hack is necessary because Doxygen doesn't
           tell us what is \internal and what isn't. -->
      <xsl:when test="contains(detaileddescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(briefdescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(inbodydescription/para, 'INTERNAL ONLY')"/>

      <xsl:when test="@kind='file'">
        <xsl:call-template name="file"/>
      </xsl:when>
      <xsl:when test="@kind='namespace'">
        <xsl:call-template name="namespace">
          <xsl:with-param name="with-namespace-refs" 
            select="$with-namespace-refs"/>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@kind='class'">
        <xsl:call-template name="class">
          <xsl:with-param name="class-key" select="'class'"/>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@kind='struct'">
        <xsl:call-template name="class">
          <xsl:with-param name="class-key" select="'struct'"/>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@kind='union'">
        <xsl:call-template name="class">
          <xsl:with-param name="class-key" select="'union'"/>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="kind-error-message">
          <xsl:with-param name="message" select="'Cannot handle compounddef'"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="namespace">
    <!-- The set of innernamespace nodes that limits our search -->
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:variable name="fullname" select="string(compoundname)"/>

    <xsl:if test="$with-namespace-refs[string(text())=$fullname]
                  and not(contains($fullname, $boost.doxygen.detailns))">
      <!-- Namespace without the prefix -->
      <xsl:variable name="rest">
        <xsl:call-template name="strip-qualifiers">
          <xsl:with-param name="name" select="compoundname"/>
        </xsl:call-template>
      </xsl:variable>
      
      <!-- Grab only the namespace name, not any further nested namespaces -->
      <xsl:variable name="name">
        <xsl:choose>
          <xsl:when 
            test="contains($rest, '::')">
            <xsl:value-of select="substring-before($rest, '::')"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$rest"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      
      <namespace>
        <xsl:attribute name="name">
          <xsl:value-of select="$name"/>
        </xsl:attribute>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
        
        <xsl:apply-templates>
          <xsl:with-param name="with-namespace-refs" 
            select="$with-namespace-refs"/>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </namespace>
      <xsl:text>&#10;</xsl:text><!-- Newline -->
    </xsl:if>
  </xsl:template>

  <xsl:template name="class">
    <xsl:param name="class-key"/>
    <xsl:param name="in-file"/>
    <xsl:param name="with-namespace-refs"/>

    <xsl:if test="string(location/attribute::file)=$in-file">
    
      <!-- The short name of this class -->
      <xsl:variable name="name-with-spec">
        <xsl:call-template name="strip-qualifiers">
          <xsl:with-param name="name" select="compoundname"/>
        </xsl:call-template>
      </xsl:variable>
      
      <xsl:variable name="name">
        <xsl:choose>
          <xsl:when test="contains($name-with-spec, '&lt;')">
            <xsl:value-of select="substring-before($name-with-spec, '&lt;')"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$name-with-spec"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>

      <xsl:variable name="specialization">
        <xsl:if test="contains($name-with-spec, '&lt;')">
          <xsl:variable name="spec-with-gt" 
            select="substring-after($name-with-spec, '&lt;')"/>
          <xsl:value-of select="substring($spec-with-gt, 1, 
                                          string-length($spec-with-gt)-1)"/>
        </xsl:if>
      </xsl:variable>

      <xsl:variable name="actual-class-key">
        <xsl:value-of select="$class-key"/>
        <xsl:if test="string-length($specialization) &gt; 0">
          <xsl:text>-specialization</xsl:text>
        </xsl:if>
      </xsl:variable>

      <xsl:element name="{$actual-class-key}">
        <xsl:attribute name="name">
          <xsl:value-of select="$name"/>
        </xsl:attribute>
        
        <xsl:apply-templates select="templateparamlist" mode="template"/>

        <xsl:if test="string-length($specialization) &gt; 0">
          <specialization>
            <xsl:call-template name="specialization">
              <xsl:with-param name="specialization" select="$specialization"/>
            </xsl:call-template>
          </specialization>
        </xsl:if>

        <xsl:apply-templates select="basecompoundref" mode="inherit"/>

        <xsl:apply-templates select="briefdescription" mode="passthrough"/>
        <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
        <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>  
        </xsl:apply-templates>
      </xsl:element>
    </xsl:if>
  </xsl:template>

  <xsl:template name="enum">
    <xsl:param name="in-file"/>

    <xsl:if test="string(location/attribute::file)=$in-file">
      <xsl:variable name="name">
        <xsl:call-template name="strip-qualifiers">
          <xsl:with-param name="name" select="name"/>
        </xsl:call-template>
      </xsl:variable>
      
      <enum>
        <xsl:attribute name="name">
          <xsl:value-of select="$name"/>
        </xsl:attribute>
        
        <xsl:apply-templates select="enumvalue"/>
        
        <xsl:apply-templates select="briefdescription" mode="passthrough"/>
        <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
        <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
      </enum>
      <xsl:text>&#10;</xsl:text><!-- Newline -->
    </xsl:if>
  </xsl:template>

  <xsl:template match="enumvalue">
    <xsl:choose>
      <!-- If the string INTERNAL ONLY is in the description, don't
           emit this entity. This hack is necessary because Doxygen doesn't
           tell us what is \internal and what isn't. -->
      <xsl:when test="contains(detaileddescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(briefdescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(inbodydescription/para, 'INTERNAL ONLY')"/>
      <xsl:otherwise>
  
        <enumvalue>
          <xsl:attribute name="name">
            <xsl:value-of select="name"/>
          </xsl:attribute>

          <xsl:if test="initializer">
            <default>
              <xsl:apply-templates select="initializer/*|initializer/text()" mode="passthrough"/>
            </default>
          </xsl:if>

          <xsl:apply-templates select="briefdescription" mode="passthrough"/>
          <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
          <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
        </enumvalue>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="doxygen.include.header.rec">
    <xsl:param name="name"/>
    <xsl:param name="header-list" select="$boost.doxygen.headers"/>

    <xsl:choose>
      <xsl:when test="contains($header-list, ' ')">
        <xsl:variable name="header" 
          select="substring-before($header-list, ' ')"/>
        <xsl:variable name="rest" select="substring-after($header-list, ' ')"/>

        <xsl:choose>
          <xsl:when test="$name=$header">
            <xsl:text>yes</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="doxygen.include.header.rec">
              <xsl:with-param name="name" select="$name"/>
              <xsl:with-param name="header-list" select="$rest"/>
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:when test="$name=$header-list">
        <xsl:text>yes</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="doxygen.include.header">
    <xsl:param name="name"/>
    
    <xsl:if test="$boost.doxygen.headers=''">
      <xsl:text>yes</xsl:text>
    </xsl:if>
    <xsl:if test="not($boost.doxygen.headers='')">
      <xsl:call-template name="doxygen.include.header.rec">
        <xsl:with-param name="name" select="$name"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <xsl:template name="file">
    <xsl:variable name="include-header">
      <xsl:call-template name="doxygen.include.header">
        <xsl:with-param name="name" select="string(compoundname)"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$include-header='yes'">
      <header>
        <xsl:attribute name="name">
          <xsl:call-template name="shorten.header.name">
            <xsl:with-param name="header" select="location/attribute::file"/>
          </xsl:call-template>
        </xsl:attribute>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
        
        <xsl:if test="briefdescription/*|detaileddescription/*|inbodydescription/*">
          <xsl:apply-templates select="briefdescription/*" mode="passthrough"/>
          <xsl:apply-templates select="detaileddescription/*" mode="passthrough"/>
          <xsl:apply-templates select="inbdoydescription/*" mode="passthrough"/>
        </xsl:if>
        
        <xsl:apply-templates mode="toplevel">
          <xsl:with-param name="with-namespace-refs"
            select="innernamespace"/>
          <xsl:with-param name="in-file" select="location/attribute::file"/>
        </xsl:apply-templates>
      </header>
      <xsl:text>&#10;</xsl:text><!-- Newline -->
    </xsl:if>
  </xsl:template>

  <xsl:template name="shorten.header.name">
    <xsl:param name="header"/>

    <xsl:variable name="prefix">
      <xsl:value-of select="concat($boost.doxygen.header.prefix, '/')"/>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="contains($header, $prefix)">
        <xsl:variable name="rest" select="substring-after($header, $prefix)"/>
        <xsl:choose>
          <xsl:when test="contains($rest, $prefix)">
            <xsl:call-template name="shorten.header.name">
              <xsl:with-param name="header" select="$rest"/>
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$prefix"/>
            <xsl:value-of select="$rest"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$header"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <xsl:template match="innernamespace">
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:apply-templates select="key('compounds-by-id', @refid)">
      <xsl:with-param name="with-namespace-refs"
        select="$with-namespace-refs"/>
      <xsl:with-param name="in-file" select="$in-file"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="innernamespace" mode="toplevel">
    <!-- The set of innernamespace nodes that limits our search -->
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <!-- The full name of the namespace we are referring to -->
    <xsl:variable name="fullname" 
      select="string(key('compounds-by-id', @refid)/compoundname)"/>

    <!-- Only pass on top-level namespaces -->
    <xsl:if test="not(contains($fullname, '::'))">
      <xsl:apply-templates select="key('compounds-by-id', @refid)">
        <xsl:with-param name="with-namespace-refs" 
          select="$with-namespace-refs"/>
        <xsl:with-param name="in-file" select="$in-file"/>
      </xsl:apply-templates>
    </xsl:if>
  </xsl:template>

  <xsl:template match="sectiondef" mode="toplevel">
    <xsl:param name="in-file" select="''"/>

    <xsl:apply-templates mode="toplevel"
                         select="memberdef[generate-id() =
                                 generate-id(key('members-by-id', @id))]">
      <xsl:with-param name="in-file" select="$in-file"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="memberdef" mode="toplevel">
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:choose>
      <!-- If the string INTERNAL ONLY is in the description, don't
           emit this entity. This hack is necessary because Doxygen doesn't
           tell us what is \internal and what isn't. -->
      <xsl:when test="contains(detaileddescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(briefdescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(inbodydescription/para, 'INTERNAL ONLY')"/>

      <xsl:when test="@kind='define'">
        <macro>
          <xsl:attribute name="name">
            <xsl:value-of select="name/text()"/>
          </xsl:attribute>

          <xsl:if test="param">
            <xsl:attribute name="kind">
              <xsl:value-of select="'functionlike'"/>
            </xsl:attribute>
          </xsl:if>

          <xsl:for-each select="param">
            <xsl:variable name="name" select="defname/text()"/>
            <macro-parameter>
              <xsl:attribute name="name">
                <xsl:value-of select="defname/text()"/>
              </xsl:attribute>
              <xsl:variable name="params"
                            select="../detaileddescription/para/parameterlist"/>
              <xsl:variable name="description" select="$params/parameteritem/
                            parameternamelist/parametername[text() = $name]/../../parameterdescription/para"/>
              <xsl:if test="$description">
                <description>
                  <xsl:apply-templates select="$description" mode="passthrough"/>
                </description>
              </xsl:if>
            </macro-parameter>
          </xsl:for-each>

          <xsl:apply-templates select="briefdescription" mode="passthrough"/>
          <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
          <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
        </macro>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
      </xsl:when>

      <xsl:when test="@kind='function'">
        <xsl:call-template name="function">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:when test="@kind='typedef'">
        <xsl:call-template name="typedef">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:when test="@kind='variable'">
        <xsl:call-template name="variable" />
      </xsl:when>

      <xsl:when test="@kind='enum'">
        <xsl:call-template name="enum" />
      </xsl:when>

      <xsl:otherwise>
        <xsl:call-template name="kind-error-message">
          <xsl:with-param name="message" select="'Cannot handle toplevel memberdef element'"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="innerclass" mode="toplevel">
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:variable name="name">
      <xsl:call-template name="strip-qualifiers">
        <xsl:with-param name="name" select="."/>
      </xsl:call-template>
    </xsl:variable>

    <!-- Only process this if it is indeed global -->
    <xsl:if test=".=$name">
      <xsl:apply-templates select="key('compounds-by-id', @refid)">
        <xsl:with-param name="with-namespace-refs" 
          select="$with-namespace-refs"/>
        <xsl:with-param name="in-file" select="$in-file"/>
      </xsl:apply-templates>
    </xsl:if>
  </xsl:template>

  <xsl:template match="innerclass">
    <xsl:param name="with-namespace-refs"/>
    <xsl:param name="in-file"/>

    <xsl:apply-templates select="key('compounds-by-id', @refid)">
      <xsl:with-param name="with-namespace-refs" 
        select="$with-namespace-refs"/>
      <xsl:with-param name="in-file" select="$in-file"/>
    </xsl:apply-templates>
  </xsl:template>

  <!-- Classes -->
  <xsl:template match="templateparamlist" mode="template">
    <template>
      <xsl:apply-templates mode="template"/>
    </template>
  </xsl:template>

  <xsl:template match="param" mode="template">
    <xsl:choose>
      <xsl:when test="string(type)='class' or string(type)='typename'">
        <xsl:variable name="name" select="normalize-space(string(declname))"/>
        <template-type-parameter>
          <xsl:attribute name="name">
            <xsl:value-of select="normalize-space(string(declname))"/>
          </xsl:attribute>
          <xsl:if test="defval">
            <default>
              <xsl:apply-templates select="defval/*|defval/text()" 
                mode="passthrough"/>
            </default>
          </xsl:if>
          <xsl:for-each select="../../detaileddescription//parameterlist[@kind='templateparam']/parameteritem">
            <xsl:if test="string(parameternamelist/parametername)=$name">
              <purpose>
                <xsl:apply-templates select="parameterdescription/para" mode="passthrough"/>
              </purpose>
            </xsl:if>
          </xsl:for-each>
        </template-type-parameter>
      </xsl:when>
      <!-- Doxygen 1.5.8 generates odd xml for template type parameters.
           This deals with that -->
      <xsl:when test="not(declname) and
        (starts-with(string(type), 'class ') or starts-with(string(type), 'typename '))">
        <template-type-parameter>
          <xsl:variable name="name">
            <xsl:value-of select="normalize-space(substring-after(string(type), ' '))"/>
          </xsl:variable>
          <xsl:attribute name="name">
            <xsl:value-of select="$name"/>
          </xsl:attribute>
          <xsl:if test="defval">
            <default>
              <xsl:apply-templates select="defval/*|defval/text()" 
                mode="passthrough"/>
            </default>
          </xsl:if>
          <xsl:for-each select="../../detaileddescription//parameterlist[@kind='templateparam']/parameteritem">
            <xsl:if test="string(parameternamelist/parametername)=$name">
              <purpose>
                <xsl:apply-templates select="parameterdescription/para" mode="passthrough"/>
              </purpose>
            </xsl:if>
          </xsl:for-each>
        </template-type-parameter>
      </xsl:when>
      <xsl:otherwise>
        <template-nontype-parameter>
          <xsl:variable name="name">
            <xsl:value-of select="normalize-space(string(declname))"/>
          </xsl:variable>
          <xsl:attribute name="name">
            <xsl:value-of select="$name"/>
          </xsl:attribute>
          <type>
            <xsl:apply-templates select="type"/>
          </type>
          <xsl:if test="defval">
            <default>
              <xsl:apply-templates select="defval/*|defval/text()" 
                mode="passthrough"/>
            </default>
          </xsl:if>
          <xsl:for-each select="../../detaileddescription//parameterlist[@kind='templateparam']/parameteritem">
            <xsl:if test="string(parameternamelist/parametername)=$name">
              <purpose>
                <xsl:apply-templates select="parameterdescription/para" mode="passthrough"/>
              </purpose>
            </xsl:if>
          </xsl:for-each>
        </template-nontype-parameter>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="templateparamlist"/>

  <!-- "Parse" a specialization from part of a name -->
  <xsl:template name="specialization">
    <xsl:param name="specialization"/>

    <xsl:choose>
      <xsl:when test="contains($specialization, ',')">
        <template-arg>
          <xsl:value-of 
            select="normalize-space(substring-before($specialization, ','))"/>
        </template-arg>
        <xsl:call-template name="specialization">
          <xsl:with-param name="specialization" 
            select="substring-after($specialization, ',')"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <template-arg>
          <xsl:value-of select="normalize-space($specialization)"/>
        </template-arg>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Inheritance -->
  <xsl:template match="basecompoundref" mode="inherit">
    <xsl:choose>
      <xsl:when test="contains(string(.), $boost.doxygen.detail)"/>
      <xsl:otherwise>
        <inherit>
          <!-- Access specifier for inheritance -->
          <xsl:attribute name="access">
            <xsl:value-of select="@prot"/>
          </xsl:attribute>
          <!-- TBD: virtual vs. non-virtual inheritance -->

          <xsl:apply-templates mode="passthrough"/>
        </inherit>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="basecompoundref"/>

  <!-- Skip over sections: they aren't very useful at all -->
  <xsl:template match="sectiondef">
    <xsl:param name="in-file" select="''"/>

    <xsl:choose>
      <xsl:when test="@kind='public-static-func'">
        <!-- TBD: pass on the fact that these are static functions -->
        <method-group name="public static functions">
          <xsl:text>&#10;</xsl:text><!-- Newline -->
          <xsl:apply-templates>
            <xsl:with-param name="in-section" select="true()"/>
            <xsl:with-param name="in-file" select="$in-file"/>
          </xsl:apply-templates>
        </method-group>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
      </xsl:when>
      <xsl:when test="@kind='protected-static-func'">
        <!-- TBD: pass on the fact that these are static functions -->
        <method-group name="protected static functions">
          <xsl:text>&#10;</xsl:text><!-- Newline -->
          <xsl:apply-templates>
            <xsl:with-param name="in-section" select="true()"/>
            <xsl:with-param name="in-file" select="$in-file"/>
          </xsl:apply-templates>
        </method-group>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
      </xsl:when>
      <xsl:when test="@kind='private-static-func'">
        <!-- TBD: pass on the fact that these are static functions -->
        <method-group name="private static functions">
          <xsl:text>&#10;</xsl:text><!-- Newline -->
          <xsl:apply-templates>
            <xsl:with-param name="in-section" select="true()"/>
            <xsl:with-param name="in-file" select="$in-file"/>
          </xsl:apply-templates>
        </method-group>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
      </xsl:when>
      <xsl:when test="@kind='public-func'">
        <xsl:variable name="members" select="./memberdef"/>
        <xsl:variable name="num-internal-only">
          <xsl:value-of
            select="count($members[contains(detaileddescription/para,
                                  'INTERNAL ONLY')])"/>
        </xsl:variable>
        <xsl:if test="$num-internal-only &lt; count($members)">
          <method-group name="public member functions">
            <xsl:text>&#10;</xsl:text><!-- Newline -->
            <xsl:apply-templates>
              <xsl:with-param name="in-section" select="true()"/>
              <xsl:with-param name="in-file" select="$in-file"/>
            </xsl:apply-templates>
          </method-group>
          <xsl:text>&#10;</xsl:text><!-- Newline -->
          <xsl:apply-templates/>
        </xsl:if>
      </xsl:when>
      <xsl:when test="@kind='protected-func'">
        <method-group name="protected member functions">
          <xsl:text>&#10;</xsl:text><!-- Newline -->
          <xsl:apply-templates>
            <xsl:with-param name="in-section" select="true()"/>
            <xsl:with-param name="in-file" select="$in-file"/>
          </xsl:apply-templates>
        </method-group>
        <xsl:text>&#10;</xsl:text><!-- Newline -->
        <xsl:apply-templates/>
      </xsl:when>
      <xsl:when test="@kind='private-func'">
        <xsl:variable name="members" select="./memberdef"/>
        <xsl:variable name="num-internal-only">
          <xsl:value-of 
            select="count($members[contains(detaileddescription/para,
                                  'INTERNAL ONLY')])"/>
        </xsl:variable>
        <xsl:if test="$num-internal-only &lt; count($members)">
          <method-group name="private member functions">
            <xsl:text>&#10;</xsl:text><!-- Newline -->
            <xsl:apply-templates>
              <xsl:with-param name="in-section" select="true()"/>
              <xsl:with-param name="in-file" select="$in-file"/>
            </xsl:apply-templates>
          </method-group>
          <xsl:text>&#10;</xsl:text><!-- Newline -->
        </xsl:if>
        <xsl:apply-templates/>
      </xsl:when>
      <xsl:when test="@kind='friend'">
        <xsl:if test="./memberdef/detaileddescription/para or ./memberdef/briefdescription/para">
          <method-group name="friend functions">
            <xsl:text>&#10;</xsl:text><!-- Newline -->
            <xsl:apply-templates>
              <xsl:with-param name="in-section" select="true()"/>
              <xsl:with-param name="in-file" select="$in-file"/>
            </xsl:apply-templates>
          </method-group>
          <xsl:text>&#10;</xsl:text><!-- Newline -->
        </xsl:if>
      </xsl:when>
      <xsl:when test="@kind='public-static-attrib' or @kind='public-attrib'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='public-type'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='private-type'">
        <!--skip private members-->
      </xsl:when>
      <xsl:when test="@kind='private-static-attrib' or @kind='private-attrib'">
        <!--skip private members-->
      </xsl:when>
      <xsl:when test="@kind='func'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='typedef'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='var'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='enum'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind='user-defined'">
        <xsl:apply-templates>
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:when test="@kind=''">
        <xsl:apply-templates select="memberdef[generate-id() =
                                     generate-id(key('members-by-id', @id))]">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="kind-error-message">
          <xsl:with-param name="message" select="'Cannot handle sectiondef'"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Handle member definitions -->
  <xsl:template match="memberdef">
    <!-- True when we're inside a section -->
    <xsl:param name="in-section" select="false()"/>
    <xsl:param name="in-file" select="''"/>

    <xsl:choose>
      <!-- If the string INTERNAL ONLY is in the description, don't
           emit this entity. This hack is necessary because Doxygen doesn't
           tell us what is \internal and what isn't. -->
      <xsl:when test="contains(detaileddescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(briefdescription/para, 'INTERNAL ONLY')"/>
      <xsl:when test="contains(inbodydescription/para, 'INTERNAL ONLY')"/>

      <xsl:when test="@kind='typedef'">
        <xsl:call-template name="typedef">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@kind='function'">
        <xsl:choose>
          <xsl:when test="ancestor::compounddef/attribute::kind='namespace'">
            <xsl:call-template name="function">
              <xsl:with-param name="in-file" select="$in-file"/>
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <!-- We are in a class -->
            <!-- The name of the class we are in -->
            <xsl:variable name="in-class-full">
              <xsl:call-template name="strip-qualifiers">
                <xsl:with-param name="name" 
                  select="string(ancestor::compounddef/compoundname/text())"/>
              </xsl:call-template>
            </xsl:variable>

            <xsl:variable name ="in-class">
              <xsl:choose>
                <xsl:when test="contains($in-class-full, '&lt;')">
                  <xsl:value-of select="substring-before($in-class-full, '&lt;')"/>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="$in-class-full"/>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:variable>
            
            <xsl:choose>
              <xsl:when test="string(name/text())=$in-class">
                <xsl:if test="not ($in-section)">
                  <xsl:call-template name="constructor"/>
                </xsl:if>
              </xsl:when>
              <xsl:when test="string(name/text())=concat('~',$in-class)">
                <xsl:if test="not ($in-section)">
                  <xsl:call-template name="destructor"/>
                </xsl:if>
              </xsl:when>
              <xsl:when test="string(name/text())='operator='">
                <xsl:if test="not ($in-section)">
                  <xsl:call-template name="copy-assignment"/>
                </xsl:if>
              </xsl:when>
              <xsl:when test="normalize-space(string(type))=''
                              and contains(name/text(), 'operator ')">
                <xsl:if test="$in-section">
                  <xsl:call-template name="conversion-operator"/>
                </xsl:if>
              </xsl:when>
              <xsl:otherwise>
                <xsl:if test="$in-section">
                  <xsl:call-template name="method"/>
                </xsl:if>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:when test="@kind='friend'">
        <xsl:if test="./detaileddescription/para or ./briefdescription/para">
          <xsl:call-template name="method"/>
        </xsl:if>
      </xsl:when>
      <xsl:when test="@kind='enum'">
        <xsl:call-template name="enum">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="@kind='variable'">
        <xsl:call-template name="variable">
          <xsl:with-param name="in-file" select="$in-file"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="kind-error-message">
          <xsl:with-param name="message" select="'Cannot handle memberdef element'"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Display typedefs -->
  <xsl:template name="typedef">
    <xsl:param name="in-file" select="''"/>

    <xsl:if test="string(location/attribute::file)=$in-file">
      <!-- TBD: Handle public/protected/private -->
      <typedef>
        <!-- Name of the type -->
        <xsl:attribute name="name">
          <xsl:value-of select="name/text()"/>
        </xsl:attribute>
        
        <xsl:apply-templates select="briefdescription" mode="passthrough"/>
        <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
        <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
        
        <type><xsl:apply-templates select="type"/></type>
      </typedef>
      <xsl:text>&#10;</xsl:text><!-- Newline -->
    </xsl:if>
  </xsl:template>

  <!-- Handle function parameters -->
  <xsl:template match="param" mode="function">
    <parameter>
      <!-- Parameter name -->
      <xsl:attribute name="name">
        <xsl:value-of select="normalize-space(declname/text())"/>
      </xsl:attribute>

      <!-- Parameter type -->
      <paramtype><xsl:apply-templates select="type"/></paramtype>

      <!-- Default argument -->
      <xsl:if test="defval">
        <default>
          <xsl:choose>
            <xsl:when test="contains(string(defval), $boost.doxygen.detail)">
              <emphasis>unspecified</emphasis>
            </xsl:when>
            <xsl:otherwise>
              <xsl:apply-templates select="defval/*|defval/text()" 
                mode="passthrough"/>
            </xsl:otherwise>
          </xsl:choose>

        </default>
      </xsl:if>

      <!-- Parameter description -->
      <xsl:variable name="name">
        <xsl:value-of select="normalize-space(declname/text())"/>
      </xsl:variable>

      <xsl:apply-templates select="../*[self::detaileddescription or self::inbodydescription]//parameterlist[attribute::kind='param']/*"
        mode="parameter.description">
        <xsl:with-param name="name">
          <xsl:value-of select="$name"/>
        </xsl:with-param>
      </xsl:apply-templates>
    </parameter>
  </xsl:template>

  <xsl:template match="parameteritem" mode="parameter.description">
    <!-- The parameter name we are looking for -->
    <xsl:param name="name"/>
    
    <xsl:if test="string(parameternamelist/parametername) = $name">
      <description>
        <xsl:apply-templates select="parameterdescription/para" mode="passthrough"/>
      </description>
    </xsl:if>
  </xsl:template>

  <!-- For older versions of Doxygen, which didn't use parameteritem -->
  <xsl:template match="parameterdescription" mode="parameter.description">
    <!-- The parameter name we are looking for -->
    <xsl:param name="name"/>
    
    <!-- The parametername node associated with this description -->
    <xsl:variable name="name-node" select="preceding-sibling::*[1]"/>

    <xsl:if test="string($name-node/text()) = $name">
      <description>
        <xsl:apply-templates select="para" mode="passthrough"/>
      </description>
    </xsl:if>
  </xsl:template>

  <xsl:template name="function.attributes">

    <!-- argsstring = '(arguments) [= delete] [= default] [constexpt]' -->
    <xsl:variable name="extra-qualifiers-a">
      <xsl:if test="contains(argsstring/text(), '(')">
        <xsl:call-template name="strip-brackets">
          <xsl:with-param name="text" select="substring-after(argsstring/text(), '(')" />
        </xsl:call-template>
      </xsl:if>
    </xsl:variable>
    <xsl:variable name="extra-qualifiers">
      <xsl:if test="$extra-qualifiers-a">
        <xsl:value-of select="concat(' ', normalize-space($extra-qualifiers-a), ' ')" />
      </xsl:if>
    </xsl:variable>

    <!-- CV Qualifiers -->
    <!-- Plus deleted and defaulted function markers as they're not properly
         supported in boostbook -->

    <!-- noexcept is complicated because is can have parameters.
         TODO: should really remove the noexcept parameters before doing
               anything else. -->
    <xsl:variable name="noexcept">
      <xsl:choose>
        <xsl:when test="contains($extra-qualifiers, ' noexcept(')">
          <xsl:call-template name="noexcept-if">
            <xsl:with-param name="condition" select="substring-after($extra-qualifiers, ' noexcept(')" />
          </xsl:call-template>
        </xsl:when>

        <xsl:when test="contains($extra-qualifiers, ' BOOST_NOEXCEPT_IF(')">
          <xsl:call-template name="noexcept-if">
            <xsl:with-param name="condition" select="substring-after($extra-qualifiers, ' BOOST_NOEXCEPT_IF(')" />
          </xsl:call-template>
        </xsl:when>

        <xsl:when test="contains($extra-qualifiers, ' noexcept ') or contains($extra-qualifiers, ' BOOST_NOEXCEPT ')">
          <xsl:value-of select="'noexcept '" />
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <!-- Calculate constexpr now, so that we can avoid it getting confused
         with const -->
    <xsl:variable name="constexpr" select="
        contains($extra-qualifiers, ' const expr ') or
        contains($extra-qualifiers, ' BOOST_CONSTEXPR ') or
        contains($extra-qualifiers, ' BOOST_CONSTEXPR_OR_CONST ')" />

    <!-- The 'substring' trick includes the string if the condition is true -->
    <xsl:variable name="cv-qualifiers" select="normalize-space(concat(
        substring('constexpr ', 1, 999 * $constexpr),
        substring('const ', 1, 999 * (not($constexpr) and @const='yes')),
        substring('volatile ', 1, 999 * (@volatile='yes' or contains($extra-qualifiers, ' volatile '))),
        $noexcept,
        substring('= delete ', 1, 999 * contains($extra-qualifiers, ' =delete ')),
        substring('= default ', 1, 999 * contains($extra-qualifiers, ' =default ')),
        substring('= 0 ', 1, 999 * (@virt = 'pure-virtual')),
        ''))" />

    <!-- Specifiers -->
    <xsl:variable name="specifiers" select="normalize-space(concat(
        substring('explicit ', 1, 999 * (@explicit = 'yes')),
        substring('virtual ', 1, 999 * (
            @virtual='yes' or @virt='virtual' or @virt='pure-virtual')),
        substring('static ', 1, 999 * (@static = 'yes')),
        ''))" />

    <xsl:if test="$cv-qualifiers">
      <xsl:attribute name="cv">
        <xsl:value-of select="$cv-qualifiers" />
      </xsl:attribute>
    </xsl:if>

    <xsl:if test="$specifiers">
      <xsl:attribute name="specifiers">
        <xsl:value-of select="$specifiers" />
      </xsl:attribute>
    </xsl:if>

  </xsl:template>

  <!-- $condition = string after the opening bracket of the condition -->
  <xsl:template name="noexcept-if">
    <xsl:param name="condition"/>

    <xsl:variable name="trailing">
      <xsl:call-template name="strip-brackets">
        <xsl:with-param name="text" select="$condition" />
      </xsl:call-template>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="string-length($trailing)">
        <xsl:value-of select="concat(
            'noexcept(',
            substring($condition, 1, string-length($condition) - string-length($trailing)),
            ') ')" />
      </xsl:when>
      <xsl:otherwise>
        <!-- Something has gone wrong so: -->
        <xsl:value-of select="'noexcept(condition) '" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- $text = substring after the opening bracket -->
  <xsl:template name="strip-brackets">
    <xsl:param name="text"/>

    <xsl:if test="contains($text, ')')">
      <xsl:variable name="prefix1" select="substring-before($text, ')')" />
      <xsl:variable name="prefix2" select="substring($prefix1, 1,
          string-length(substring-before($prefix1, '(')) +
          999 * not(contains($prefix1, '(')))" />
      <xsl:variable name="prefix3" select="substring($prefix2, 1,
          string-length(substring-before($prefix2, '&quot;')) +
          999 * not(contains($prefix2, '&quot;')))" />
      <xsl:variable name="prefix" select="substring($prefix3, 1,
          string-length(substring-before($prefix3, &quot;'&quot;)) +
          999 * not(contains($prefix3, &quot;'&quot;)))" />

      <xsl:variable name="prefix-length" select="string-length($prefix)" />
      <xsl:variable name="char" select="substring($text, $prefix-length + 1, 1)" />

      <xsl:choose>
        <xsl:when test="$char=')'">
          <xsl:value-of select="substring($text, $prefix-length + 2)" />
        </xsl:when>
        <xsl:when test="$char='('">
          <xsl:variable name="text2">
            <xsl:call-template name="strip-brackets">
              <xsl:with-param name="text" select="substring($text, $prefix-length + 2)" />
            </xsl:call-template>
          </xsl:variable>
          <xsl:call-template name="strip-brackets">
            <xsl:with-param name="text" select="$text2" />
          </xsl:call-template>
        </xsl:when>
        <xsl:when test="$char=&quot;'&quot;">
          <!-- Not bothering with escapes, because this is crazy enough as it is -->
          <xsl:call-template name="strip-brackets">
            <xsl:with-param name="text" select="substring-after(substring($text, $prefix-length + 2), &quot;'&quot;)" />
          </xsl:call-template>
        </xsl:when>
        <xsl:when test="$char='&quot;'">
          <!-- Not bothering with escapes, because this is crazy enough as it is -->
          <xsl:call-template name="strip-brackets">
            <xsl:with-param name="text" select="substring-after(substring($text, $prefix-length + 2), '&quot;')" />
          </xsl:call-template>
        </xsl:when>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Handle function children -->
  <xsl:template name="function.children">
    <xsl:param name="is-overloaded" select="false()"/>

    <xsl:if test="not($is-overloaded)">
      <!-- Emit template header -->
      <xsl:apply-templates select="templateparamlist" mode="template"/>
      
      <!-- Emit function parameters -->
      <xsl:apply-templates select="param" mode="function"/>
    </xsl:if>

    <!-- The description -->
    <xsl:apply-templates select="briefdescription" mode="passthrough"/>
    <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
    <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
      
    <xsl:apply-templates 
      select="*[self::detaileddescription or self::inbodydescription]/para/simplesect[@kind='pre']"
      mode="function-clauses"/>
    <xsl:apply-templates 
      select="*[self::detaileddescription or self::inbodydescription]/para/simplesect[@kind='post']"
      mode="function-clauses"/>
    <xsl:apply-templates 
      select="*[self::detaileddescription or self::inbodydescription]/para/simplesect[@kind='return']"
      mode="function-clauses"/>
    <xsl:if test="*[self::detaileddescription or self::inbodydescription]/para/parameterlist[@kind='exception']">
      <throws>
        <xsl:apply-templates 
          select="*[self::detaileddescription or self::inbodydescription]/para/parameterlist[@kind='exception']"
          mode="function-clauses"/>
      </throws>
    </xsl:if>
  </xsl:template>

  <!-- Handle free functions -->
  <xsl:template name="function">
    <xsl:param name="in-file" select="''"/>

    <xsl:variable name="firstpara" 
      select="normalize-space(detaileddescription/para[1])"/>
    <xsl:if test="string(location/attribute::file)=$in-file
                  and 
                  not($firstpara=normalize-space($boost.doxygen.overload))">

      <xsl:variable name="next-node" select="following-sibling::*[1]"/>
      <xsl:variable name="has-overload">
        <xsl:if test="not(local-name($next-node)='memberdef')">
          false
        </xsl:if>
        <xsl:if test="not(string($next-node/name/text())=string(name/text()))">
          false
        </xsl:if>
        <xsl:if 
          test="not(normalize-space($next-node/detaileddescription/para[1])
                    =normalize-space($boost.doxygen.overload))">
          false
        </xsl:if>
      </xsl:variable>

      <xsl:choose>
        <xsl:when test="not(contains($has-overload, 'false'))">
          <overloaded-function>
            <xsl:attribute name="name">
              <xsl:call-template name="normalize-name"/>
            </xsl:attribute>

            <xsl:call-template name="overload-signatures"/>
            <xsl:call-template name="function.children">
              <xsl:with-param name="is-overloaded" select="true()"/>
            </xsl:call-template>
          </overloaded-function>
        </xsl:when>
        <xsl:otherwise>
          <function>
            <xsl:attribute name="name">
              <xsl:call-template name="normalize-name"/>
            </xsl:attribute>
            
            <!-- Return type -->
            <type><xsl:apply-templates select="type"/></type>
            
            <xsl:call-template name="function.children"/>
          </function>          
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Emit overload signatures -->
  <xsl:template name="overload-signatures">
    <xsl:param name="node" select="."/>
    <xsl:param name="name" select="string(name/text())"/>
    <xsl:param name="first" select="true()"/>

    <xsl:choose>
      <xsl:when test="not(local-name($node)='memberdef')"/>
      <xsl:when test="not(string($node/name/text())=$name)"/>
      <xsl:when test="not(normalize-space($node/detaileddescription/para[1])
                          =normalize-space($boost.doxygen.overload))
                      and not($first)"/>
      <xsl:otherwise>
        <signature>
          <type>
            <xsl:apply-templates select="$node/type"/>
          </type>
          <xsl:apply-templates select="$node/templateparamlist" 
            mode="template"/>
          <xsl:apply-templates select="$node/param" mode="function"/>
        </signature>

        <xsl:call-template name="overload-signatures">
          <xsl:with-param name="node" select="$node/following-sibling::*[1]"/>
          <xsl:with-param name="name" select="$name"/>
          <xsl:with-param name="first" select="false()"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Handle constructors -->
  <xsl:template name="constructor">
    <constructor>
      <xsl:if test="@explicit = 'yes'">
        <xsl:attribute name="specifiers">explicit</xsl:attribute>
      </xsl:if>
      <xsl:call-template name="function.attributes"/>
      <xsl:call-template name="function.children"/>
    </constructor>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Handle Destructors -->
  <xsl:template name="destructor">
    <destructor>
      <xsl:call-template name="function.children"/>
    </destructor>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Handle Copy Assignment -->
  <xsl:template name="copy-assignment">
    <copy-assignment>
      <xsl:call-template name="function.attributes"/>
      <!-- Return type -->
      <xsl:element name="type">
        <xsl:apply-templates select="type"/>
      </xsl:element>

      <xsl:call-template name="function.children"/>
    </copy-assignment>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Handle conversion operator -->
  <xsl:template name="conversion-operator">
    <method>
      <xsl:attribute name="name">
        <xsl:text>conversion-operator</xsl:text>
      </xsl:attribute>
      <xsl:call-template name="function.attributes"/>

      <!-- Conversion type -->
      <type>
        <xsl:value-of select="substring-after(name/text(), 'operator ')"/>
      </type>

      <xsl:call-template name="function.children"/>
    </method>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Handle methods -->
  <xsl:template name="method">
    <method>
      <xsl:attribute name="name">
        <xsl:value-of select="name/text()"/>
      </xsl:attribute>
      <xsl:call-template name="function.attributes"/>

      <!-- Return type -->
      <xsl:element name="type">
        <xsl:apply-templates select="type"/>
      </xsl:element>

      <xsl:call-template name="function.children"/>
    </method>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
  </xsl:template>

  <!-- Handle member variables -->
  <xsl:template name="variable">
    <xsl:param name="in-file"/>
    <xsl:if test="string(location/attribute::file)=$in-file">
    <data-member>
      <xsl:attribute name="name">
        <xsl:value-of select="name/text()"/>
      </xsl:attribute>

      <!-- Specifiers -->
      <xsl:if test="@static = 'yes'">
        <xsl:attribute name="specifiers">static</xsl:attribute>
      </xsl:if>
      <xsl:if test="@mutable = 'yes'">
        <xsl:attribute name="specifiers">mutable</xsl:attribute>
      </xsl:if>

      <type>
        <xsl:apply-templates select="type"/>
      </type>

      <xsl:apply-templates select="briefdescription" mode="passthrough"/>
      <xsl:apply-templates select="detaileddescription" mode="passthrough"/>
      <xsl:apply-templates select="inbodydescription" mode="passthrough"/>
    </data-member>
    <xsl:text>&#10;</xsl:text><!-- Newline -->
    </xsl:if>
  </xsl:template>

  <!-- Things we ignore directly -->
  <xsl:template match="compoundname" mode="toplevel"/>
  <xsl:template match="includes|includedby|incdepgraph|invincdepgraph" mode="toplevel"/>
  <xsl:template match="programlisting" mode="toplevel"/>
  <xsl:template match="text()" mode="toplevel"/>

  <xsl:template match="text()"/>

  <!-- Passthrough of text -->
  <xsl:template match="text()" mode="passthrough">
    <xsl:value-of select="."/>
  </xsl:template>
  <xsl:template match="para" mode="passthrough">
    <para>
      <xsl:apply-templates mode="passthrough"/>
    </para>
  </xsl:template>
  <xsl:template match="copydoc" mode="passthrough">
    <xsl:apply-templates mode="passthrough"/>
  </xsl:template>
  <xsl:template match="verbatim" mode="passthrough">
    <xsl:copy-of select="node()"/>
  </xsl:template>

  <xsl:template match="para/simplesect" mode="passthrough">
    <xsl:if test="not (@kind='pre') and
                  not (@kind='return') and 
                  not (@kind='post') and
                  not (@kind='attention') and
                  not (@kind='see') and
                  not (@kind='warning') 
                  ">
      <xsl:apply-templates mode="passthrough"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="para/simplesect[@kind='note' or @kind='attention']" mode="passthrough">
    <note>
      <xsl:apply-templates mode="passthrough"/>
    </note>
  </xsl:template>

  <xsl:template match="para/simplesect[@kind='warning']" mode="passthrough">
    <warning>
      <xsl:apply-templates mode="passthrough"/>
    </warning>
  </xsl:template>

  <xsl:template match="para/simplesect[@kind='par']" mode="passthrough">
    <formalpara>
      <xsl:apply-templates mode="passthrough"/>
    </formalpara>
  </xsl:template>

  <xsl:template match="para/simplesect[@kind='see']" mode="passthrough">
    <para>
      <emphasis role="bold">
        <xsl:text>See Also:</xsl:text>
      </emphasis>
      <xsl:apply-templates mode="passthrough"/>
    </para>
  </xsl:template>

  <xsl:template match="simplesectsep" mode="passthrough">
    <xsl:apply-templates mode="passthrough"/>
  </xsl:template>

  <xsl:template match="*" mode="passthrough">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="passthrough"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="parameterlist" mode="passthrough"/>

  <xsl:template match="bold" mode="passthrough">
    <emphasis role="bold">
      <xsl:apply-templates mode="passthrough"/>
    </emphasis>
  </xsl:template>

  <xsl:template match="linebreak" mode="passthrough">
    <sbr/>
  </xsl:template>

  <xsl:template match="ndash" mode="passthrough">
    <xsl:text>&#8211;</xsl:text>
  </xsl:template>
  
  <xsl:template match="briefdescription" mode="passthrough">
    <xsl:if test="text()|*">
      <purpose>
        <xsl:apply-templates mode="purpose"/>
      </purpose>
    </xsl:if>
  </xsl:template>

  <xsl:template match="detaileddescription" mode="passthrough">
    <xsl:if test="text()|*">
      <description>
        <xsl:apply-templates mode="passthrough"/>
      </description>
    </xsl:if>
  </xsl:template>

  <xsl:template match="inbodydescription" mode="passthrough">
    <xsl:if test="text()|*">
      <description>
        <xsl:apply-templates mode="passthrough"/>
      </description>
    </xsl:if>
  </xsl:template>

  <!-- Ignore ref elements for now, as there is a lot of documentation which
       will have incorrect ref elements at the moment -->
  <xsl:template match="ref" mode="passthrough">
    <xsl:variable name="as-class" select="key('compounds-by-id', @refid)[@kind='class' or @kind='struct']"/>
    <xsl:choose>
      <xsl:when test="$as-class">
        <classname>
          <xsl:attribute name="alt">
            <xsl:value-of select="$as-class/compoundname/text()"/>
          </xsl:attribute>
          <xsl:value-of select="text()"/>
        </classname>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates mode="passthrough"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Handle function clauses -->
  <xsl:template match="simplesect" mode="function-clauses">
    <xsl:if test="@kind='pre'">
      <requires>
        <xsl:apply-templates mode="passthrough"/>
      </requires>
    </xsl:if>
    <xsl:if test="@kind='return'">
      <returns>
        <xsl:apply-templates mode="passthrough"/>
      </returns>
    </xsl:if>
    <xsl:if test="@kind='post'">
      <postconditions>
        <xsl:apply-templates mode="passthrough"/>
      </postconditions>
    </xsl:if>
  </xsl:template>

  <xsl:template match="parameterlist" mode="function-clauses">
    <xsl:if test="@kind='exception'">
      <simpara>
        <xsl:choose>
          <xsl:when test="normalize-space(.//parametername//text())='nothrow'">
            <xsl:text>Will not throw.</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <classname>
              <xsl:value-of select=".//parametername//text()"/>
            </classname>
            <xsl:text> </xsl:text>
            <xsl:apply-templates 
              select=".//parameterdescription/para/text()
                      |.//parameterdescription/para/*"
              mode="passthrough"/>
          </xsl:otherwise>
        </xsl:choose>
      </simpara>
    </xsl:if>
  </xsl:template>

  <xsl:template match="type">
    <xsl:choose>
      <xsl:when test="contains(string(.), $boost.doxygen.detail)">
        <emphasis>unspecified</emphasis>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates mode="type"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="ref" mode="type">
    <xsl:choose>
      <xsl:when test="@kindref='compound'">
        <classname>
          <xsl:value-of select="text()"/>
        </classname>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="text()"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="*" mode="type">
    <xsl:value-of select="."/>
  </xsl:template>

  <!-- Normalize the names of functions, because Doxygen sometimes
       puts in an obnoixous space. -->
  <xsl:template name="normalize-name">
    <xsl:param name="name" select="name/text()"/>

    <xsl:choose>
      <xsl:when test="contains($name, ' ')">
        <xsl:value-of select="substring-before($name, ' ')"/>
        <xsl:value-of select="substring-after($name, ' ')"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$name"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Convert HTML tables into DocBook format -->
  <xsl:template match="table" mode="passthrough">
    <informaltable>
      <tgroup>
        <xsl:attribute name="cols">
          <xsl:value-of select="@cols"/>
        </xsl:attribute>

        <tbody>
          <xsl:apply-templates mode="table"/>
        </tbody>
      </tgroup>
    </informaltable>
  </xsl:template>

  <xsl:template match="row" mode="table">
    <row>
      <xsl:apply-templates mode="table"/>
    </row>
  </xsl:template>
  
  <xsl:template match="entry" mode="table">
    <entry>
      <xsl:if test="para/center">
        <xsl:attribute name="valign">
          <xsl:value-of select="'middle'"/>
        </xsl:attribute>
        <xsl:attribute name="align">
          <xsl:value-of select="'center'"/>
        </xsl:attribute>
      </xsl:if>

      <xsl:choose>
        <xsl:when test="@thead='yes'">
          <emphasis role="bold">
            <xsl:call-template name="table-entry"/>
          </emphasis>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="table-entry"/>
        </xsl:otherwise>
      </xsl:choose>
    </entry>
  </xsl:template>

  <xsl:template name="table-entry">
    <xsl:choose>
      <xsl:when test="para/center">
        <xsl:apply-templates select="para/center/*|para/center/text()"
          mode="passthrough"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="para/*|para/text()" mode="passthrough"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Handle preformatted.
       Doxygen generates markup such as:
         <para><preformatted>void foo() {</preformatted></para>
         <para><preformatted>}</preformatted></para>
       This complicated mess tries to combine that into a single
       programlisting element.
    -->
  <xsl:template match="para[preformatted][count(preformatted)=count(*)]" mode="passthrough">
    <!-- Only do this for the first of a group of paras. -->
    <xsl:if test="not(preceding-sibling::para[1][preformatted][count(preformatted)=count(*)])">
      <programlisting>
        <!-- This node's children. -->
        <xsl:apply-templates mode="passthrough" select="./preformatted/node()"/>

        <!-- Adjacent para nodes' children.
             Based on code at: http://stackoverflow.com/a/2092144 -->
        <xsl:variable name="following-siblings" select="following-sibling::*" />
        <xsl:for-each select="following-sibling::para[preformatted][count(preformatted)=count(*)]">
          <xsl:variable name="index" select="position()"/>
          <xsl:if test="generate-id(.)=generate-id($following-siblings[$index])">
            <xsl:text>&#xa;&#xa;</xsl:text>
            <xsl:apply-templates mode="passthrough" select="./preformatted/node()"/>
          </xsl:if>
        </xsl:for-each>
      </programlisting>
    </xsl:if>
  </xsl:template>

  <!-- Remove empty preformatted elements. -->
  <xsl:template match="preformatted[not(node())]" mode="passthrough"/>

  <!-- Convert remaining to programlisting. -->
  <xsl:template match="preformatted" mode="passthrough">
    <programlisting>
      <xsl:apply-templates mode="passthrough"/>
    </programlisting>
  </xsl:template>

  <!-- Handle program listings -->
  <xsl:template match="programlisting" mode="passthrough">
    <programlisting language="c++">
      <xsl:apply-templates mode="programlisting"/>
    </programlisting>
  </xsl:template>

  <xsl:template match="highlight|codeline" mode="programlisting">
    <xsl:apply-templates mode="programlisting"/>
  </xsl:template>

  <xsl:template match="sp" mode="programlisting">
    <xsl:text> </xsl:text>
  </xsl:template>

  <xsl:template match="*" mode="programlisting">
    <xsl:apply-templates select="." mode="passthrough"/>
  </xsl:template>

  <!-- Replace top-level "para" elements with "simpara" elements in
       the purpose -->
  <xsl:template match="*" mode="purpose">
    <xsl:apply-templates mode="passthrough"/>
  </xsl:template>

  <xsl:template match="text()" mode="purpose">
    <xsl:apply-templates mode="passthrough"/>
  </xsl:template>

  <xsl:template match="para" mode="purpose">
    <xsl:apply-templates select="*|text()" mode="passthrough"/>
  </xsl:template>

  <!--
  Eric Niebler: Jan-8-2008
  Here is some 3/4-baked support for LaTeX formulas in
  Doxygen comments. Doxygen doesn't generate the PNG files
  when outputting XML. In order to use this code, you must
  run Doxygen first to generate HTML (and the PNG files for
  the formulas). You can do this in a Jamfile with 
  "doxygen foo.html : <sources, etc...> ; ", where the ".html"
  is significant. Then the png files should be copied into the 
  images/ directory (or another place relative to the html/
  directory, as specified by $boost.doxygen.formuladir XSL
  parameter). This can be done with a custom action in a 
  Jamfile. Finally, the docs can be built as normal.
  See libs/accumulators/doc/Jamfile.v2 for a working example.
  -->
  <xsl:template match="formula" mode="passthrough">
    <xsl:choose>
      <xsl:when test="substring(*|text(), 1, 2) = '\['">
        <equation>
          <title/>
          <alt>
            <xsl:value-of select="*|text()"/>
          </alt>
          <mediaobject>
            <imageobject role="html">
              <imagedata format="PNG" align="center">
                <xsl:attribute name="fileref">
                  <xsl:value-of select="concat(concat(concat($boost.doxygen.formuladir.fixed, 'form_'), @id), '.png')"/>
                </xsl:attribute>
              </imagedata>
            </imageobject>
            <textobject role="tex">
              <phrase>
                <xsl:value-of select="*|text()"/>
              </phrase>
            </textobject>
          </mediaobject>
        </equation>
      </xsl:when>
      <xsl:otherwise>
        <inlineequation>
          <alt>
            <xsl:value-of select="*|text()"/>
          </alt>
          <inlinemediaobject>
            <imageobject role="html">
              <imagedata format="PNG">
                <xsl:attribute name="fileref">
                  <xsl:value-of select="concat(concat(concat($boost.doxygen.formuladir.fixed, 'form_'), @id), '.png')"/>
                </xsl:attribute>
              </imagedata>
            </imageobject>
            <textobject role="tex">
              <phrase>
                <xsl:value-of select="*|text()"/>
              </phrase>
            </textobject>
          </inlinemediaobject>
        </inlineequation>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
 </xsl:stylesheet>
