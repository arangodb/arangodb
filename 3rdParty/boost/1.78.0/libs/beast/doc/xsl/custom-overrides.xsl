<xsl:stylesheet version="3.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  exclude-result-prefixes="xs"
  expand-text="yes">

  <xsl:variable name="doc-ref" select="'beast.ref'"/>
  <xsl:variable name="doc-ns" select="'boost::beast'"/>
  <xsl:variable name="include-private-members" select="false()"/>

  <xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/core')]"     >core.hpp</xsl:template>
  <xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/http')]"     >http.hpp</xsl:template>
  <xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/ssl')]"      >ssl.hpp</xsl:template>
  <xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/websocket')]">websocket.hpp</xsl:template>
  <xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/zlib')]"     >zlib.hpp</xsl:template>
  <xsl:template mode="convenience-header" match="@file"/>

  <xsl:variable name="emphasized-template-parameter-types" select="
    'Allocator',
    'AsyncStream',
    'AsyncReadStream',
    'AsyncWriteStream',
    'Body',
    'BufferSequence',
    'BufferSequence',  (: TODO: Was this intended to be 'BufferSequence_' ?? :)
    'CompletionCondition',
    'CompletionHandler',
    'CompletionToken',
    'ConnectCondition',
    'ConnectHandler',
    'ConstBufferSequence',
    'DynamicBuffer',
    'EndpointSequence',
    'ExecutionContext',
    'Executor',
    'Executor_',
    'Executor1',
    'Executor2',
    'Fields',
    'Handler',
    'Handler_',
    'IteratorConnectHandler',
    'MutableBufferSequence',
    'Protocol',
    'RangeConnectHandler',
    'RatePolicy',
    'ReadHandler',
    'Stream',
    'SyncStream',
    'SyncReadStream',
    'SyncWriteStream',
    'WriteHandler'
  "/>

</xsl:stylesheet>
