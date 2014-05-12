Upgrading to ArangoDB 2.1 {#Upgrading21}
========================================

@NAVIGATE_Upgrading21
@EMBEDTOC{Upgrading21TOC}

Upgrading {#Upgrading21Introduction}
====================================

Please read the following sections if you upgrade from a previous version to
ArangoDB 2.1.

Please note first that a database directory used with ArangoDB 2.0 cannot be used with
earlier versions (e.g. ArangoDB 2.0) any more. Upgrading a database directory cannot
be reverted. Therefore please make sure to create a full backup of your existing 
ArangoDB installation before performing an upgrade.

Database Directory Version Check and Upgrade {#Upgrading21VersionCheck}
-----------------------------------------------------------------------

ArangoDB will perform a database version check at startup. When ArangoDB 2.1 
encounters a database created with earlier versions of ArangoDB, it will refuse 
to start. This is intentional.

The output will then look like this:

    ....
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': Database directory version (2) is lower than server version (2.1).
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': ----------------------------------------------------------------------
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': It seems like you have upgraded the ArangoDB binary.
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': If this is what you wanted to do, please restart with the
    2014-05-12T20:32:24Z [20282] ERROR In database '_system':   --upgrade
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': option to upgrade the data in the database directory.
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': Normally you can use the control script to upgrade your database
    2014-05-12T20:32:24Z [20282] ERROR In database '_system':   /etc/init.d/arangodb stop
    2014-05-12T20:32:24Z [20282] ERROR In database '_system':   /etc/init.d/arangodb upgrade
    2014-05-12T20:32:24Z [20282] ERROR In database '_system':   /etc/init.d/arangodb start
    2014-05-12T20:32:24Z [20282] ERROR In database '_system': ----------------------------------------------------------------------
    2014-05-12T20:32:24Z [20282] FATAL Database version check failed for '_system'. Please start the server with the --upgrade option

To make ArangoDB 2.1 start with a database directory created with an
earlier ArangoDB version, you may need to invoke the upgrade procedure once.
This can be done by running ArangoDB from the command line and supplying
the `--upgrade` option:

    unix> arangod data --upgrade

where `data` is ArangoDB's main data directory. 

Note: here the same database should be specified that is also specified when arangod
is started regularly. Please do not run the `--upgrade` command on each individual
database subfolder (named `database-<some number>`).
 
For example, if you regularly start your ArangoDB server with

    unix> arangod mydatabasefolder

then running

    unix> arangod mydatabasefolder --upgrade

will perform the upgrade for the whole ArangoDB instance, including all of
its databases.

Starting with `--upgrade` will run a database version check and perform
any necessary migrations. As usual, you should create a backup of your database
directory before performing the upgrade.

The output should look like this:

    ...
    2014-05-12T20:42:13Z [20685] INFO In database '_system': upgrade successfully finished
    2014-05-12T20:42:13Z [20685] INFO database upgrade passed

Please check the output the `--upgrade` run. It may produce errors, which need to be
fixed before ArangoDB can be used properly. If no errors are present or they have been
resolved, you can start ArangoDB 2.1 regularly.

Changed Behavior {#Upgrading21ChangedBehavior}
==============================================

* The underlying code for retrieving connected edges was changed in ArangoDB 2.1. It 
  may therefore happen that connected edges are now returned in a different order than
  in previous versions of ArangoDB. This shouldn't be a problem because the order of 
  edges returned was never guaranteed in previous versions of ArangoDB either. Still
  this change might affect results if client application code queries or processes 
  just the first x out of y connected edges without a previous sorting or weighting.

* Starting with version 2.1, ArangoDB will return capitalized HTTP headers by default, 
  e.g. `Content-Length` instead of `content-length`.
  Though the HTTP specification states that headers field name are case-insensitive, 
  there might be clients or applications that expect HTTP response headers with an exact
  case. Those will need to be adjusted to be used with ArangoDB 2.1. Alternatively,
  clients or applications can send ArangoDB the `X-Arango-Version` HTTP request header
  with a version of `20000` (for version 2.0) to indicate they desire compatibility with
  ArangoDB's 2.0 header behavior. The default value for the compatibility can also be 
  set at server start, using the `--server.default-api-compatibility` option.

Removed Features {#Upgrading21RemovedFeatures}
==============================================

* The startup option `--database.remove-on-compacted` has been removed in AragnoDB 2.1.
  This option was present for debugging purposes only.

@BNAVIGATE_Upgrading21
