Upgrading to ArangoDB 1.4 {#Upgrading14}
========================================

@NAVIGATE_Upgrading14
@EMBEDTOC{Upgrading14TOC}

Upgrading {#Upgrading14Introduction}
====================================

1.4 is currently alpha, please do not use in production.

Removed Features {#Upgrading14RemovedFeatures}
==============================================

Removed or Renamed Configuration options {#Upgrading14RemovedConfiguration}
---------------------------------------------------------------------------

The following changes have been made in ArangoDB 1.4 with respect to
configuration / command-line options:

- The options `--server.admin-directory` and `--server.disable-admin-interface`
  have been removed. 
  
  In previous versions of ArangoDB, these options controlled where the static 
  files of the web admin interface were located and if the web admin interface 
  was accessible via HTTP. 
  In ArangoDB 1.4, the web admin interface has become a Foxx application and
  does not require an extra location. 

- The option `--log.filter` was renamed to `--log.source-filter`.

  This is a debugging option that should rarely be used by non-developers.
