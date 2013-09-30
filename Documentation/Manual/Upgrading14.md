Upgrading to ArangoDB 1.4 {#Upgrading14}
========================================

@NAVIGATE_Upgrading14
@EMBEDTOC{Upgrading14TOC}

Upgrading {#Upgrading14Introduction}
====================================

1.4 is currently alpha, please do not use in production.

Troubleshooting {#Upgrading14Troubleshooting}
=============================================

If you cannot find a solution here, please ask the Google-Group at 
http://groups.google.com/group/arangodb

Problem: Server does not start
------------------------------

The server will try to bind to all defined endpoints at startup. The
list of endpoints to connect to is read from the command-line, ArangoDB's
configuration file, and a file named `ENDPOINTS` in the database directory.
The `ENDPOINTS` file contains all endpoints which were added at runtime.

In case a previously defined endpoint is invalid at server start, ArangoDB
will fail on startup with a message like this:

    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://127.0.0.1:8529' for non-encrypted requests
    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://127.0.0.1:8531' for non-encrypted requests
    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://localhost:8529' for non-encrypted requests
    ...
    2013-09-24T07:09:01Z [4318] ERROR bind() failed with 98 (Address already in use)
    2013-09-24T07:09:01Z [4318] FATAL failed to bind to endpoint 'tcp://localhost:8529'. Please review your endpoints configuration.

The problem above may be obvious: in a typical setup, `127.0.0.1` will be 
the same IP address, so ArangoDB will try to bind to the same address twice
(which will fail).
Other obvious bind problems at startup may be caused by ports being used by
other programs, or IP addresses changing.


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
