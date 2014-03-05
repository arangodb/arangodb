Upgrading to ArangoDB 2.0 {#Upgrading20}
========================================

@NAVIGATE_Upgrading20
@EMBEDTOC{Upgrading20TOC}

Upgrading {#Upgrading20Introduction}
====================================

Please read the following sections if you upgrade from a pre-2.0
version of ArangoDB to ArangoDB 2.0.

ArangoDB 2.0 comes with a few changes, some of which are not 100% compatible to
ArangoDB 1.4. The incompatibilies are due to a change of some HTTP response
codes returned by a few server APIs.

Following is a list of incompatible changes with workarounds. Please read the list
carefully and adjust any client programs or processes that work with ArangoDB 
appropriately. 

Please note that a database directory used with ArangoDB 2.0 cannot be used with
earlier versions (e.g. ArangoDB 1.4) any more. Upgrading a database directory cannot
be reverted. Therefore please make sure to create a full backup of your existing 
ArangoDB installation before performing an update.

The upgrading section is closed with an instruction summary and a list of potential 
problems and @ref Upgrading14Troubleshooting "troubleshooting options" for them. 
Please consult that section if you encounter any problems during or after the upgrade.

Database Directory Version Check and Upgrade {#Upgrading20VersionCheck}
-----------------------------------------------------------------------

ArangoDB will perform a database version check at startup. This has not changed in
ArangoDB 2.0. When ArangoDB 2.0 encounters a database created with earlier 
versions of ArangoDB, it will refuse to start. This is intentional.
The output will then look like this:

    ....
    2014-03-04T17:15:08Z [28446] ERROR In database '...': Database directory version (1.4) is lower than server version (2).
    2014-03-04T17:15:08Z [28446] ERROR In database '...': ----------------------------------------------------------------------
    2014-03-04T17:15:08Z [28446] ERROR In database '...': It seems like you have upgraded the ArangoDB binary.
    2014-03-04T17:15:08Z [28446] ERROR In database '...': If this is what you wanted to do, please restart with the
    2014-03-04T17:15:08Z [28446] ERROR In database '...':   --upgrade
    2014-03-04T17:15:08Z [28446] ERROR In database '...': option to upgrade the data in the database directory.
    2014-03-04T17:15:08Z [28446] ERROR In database '...': Normally you can use the control script to upgrade your database
    2014-03-04T17:15:08Z [28446] ERROR In database '...':   /etc/init.d/arangodb stop
    2014-03-04T17:15:08Z [28446] ERROR In database '...':   /etc/init.d/arangodb upgrade
    2014-03-04T17:15:08Z [28446] ERROR In database '...':   /etc/init.d/arangodb start
    2014-03-04T17:15:08Z [28446] ERROR In database '...': ----------------------------------------------------------------------
    2014-03-04T17:15:08Z [28446] FATAL Database version check failed for '...'. Please start the server with the --upgrade option

To make ArangoDB 2.0 start with a database directory created with an
earlier ArangoDB version, you may need to invoke the upgrade procedure once.
This can be done by running ArangoDB from the command line and supplying
the `--upgrade` option:

    unix> arangod data --upgrade

where `data` is the database directory. This will run a database version and
any necessary migrations. As usual, you should create a backup of your database
directory before performing the upgrade.

The output should look like this:

    ...
    2014-03-04T17:17:29Z [28481] INFO In database '...': upgrade successfully finished
    2014-03-04T17:17:29Z [28481] INFO database upgrade passed

Please check the output the `--upgrade` run. It may produce errors, which need to be
fixed before ArangoDB can be used properly. If no errors are present or they have been
resolved, you can start ArangoDB 2.0 regularly.

Shape Collection Changes {#Upgrading20ShapeCollections}
-------------------------------------------------------

During the upgrade to 2.0, any existing collections will be converted to include the
structural shape information that was previously stored in separate shape collections.

For each collection in each database, ArangoDB will scan the `SHAPES` directory and
write out all shapes of a collection into a separate datafile in the collection directory.

The `SHAPES` directory will be removed afterwards..

This saves up to 2 MB of memory and disk space for each collection, and will make 
ArangoDB use one less file descriptor per opened collection.

Please note after converting the shapes, the database directory and collection files 
cannot be used with earlier versions of ArangoDB.

Changed Behavior {#Upgrading20ChangedBehavior}
==============================================

* Changed HTTP response code of HTTP REST API method POST `/_api/database`:

  Prior to ArangoDB 2.0, creating a database returned a status code of 200 (Ok). 
  ArangoDB 2.0 will instead return HTTP status code 201 (Created) when a database was
  successfully created.

  To enforce the old behavior, clients can send ArangoDB 2.0 the HTTP header
  `X-Arango-Version` with a value of `10400`. ArangoDB 2.0 will then still return
  HTTP 200 (Ok) when creating a database.

* The AQL `TRAVERSAL` function was changed to throw an error if an invalid value
  is specified for any of the following (optional) attributes:
  - `strategy` 
  - `order`
  - `itemOrder`
  
  Omitting these attributes in a call to `TRAVERSAL` is not considered an error, but
  specifying an invalid value for any of these attributes will make the AQL query fail.

* The `shapefiles` attribute returned by the `collection.figures()` method or the
  HTTP REST API method GET `/_api/collection/<name>/figures` will always contain a
  `count` value of `0` and a `fileSize` of `0` in ArangoDB 2.0.
  
  This is because there are no separate shape collections any longer.

* arangod and arangosh now determine the paths to the built-in JavaScript files by 
  looking at the option `--javascript.startup-directory`. All other paths to built-in 
  JavaScript files are derived automatically from this option.

  This change has made the following command line options of arangod and arangosh useless 
  in version 2.0: 

  * `--javascript.modules-path` (arangod and arangosh)
  * `--javascript.package-path` (arangod and arangosh)
  * `--javascript.action-directory` (arangod only)

  These options can be removed from any invoking scripts for ArangoDB 2.0.

  Note that `--javascript.startup-directory` is still required for arangod and arangosh.
  arangod will also still require the option `--javascript.app-path` at startup.

Removed Features {#Upgrading20RemovedFeatures}
==============================================

* The undocumented and not advertised method `collection.saveOrReplace()` has been
  removed in ArangoDB 2.0.

* The undocumented and not advertised index type priority queue has been removed in
  ArangoDB 2.0.

* The undocumented and not advertised HTTP REST API method GET `/_admin/database-name`
  has been removed in ArangoDB 2.0.

* The undocumented and not advertised HTTP REST API method PUT `/_api/simple/BY-EXAMPLE-HASH`
  has been removed in ArangoDB 2.0.

* The command-line option `--log.format` has been removed in ArangoDB 2.0.

@BNAVIGATE_Upgrading20
