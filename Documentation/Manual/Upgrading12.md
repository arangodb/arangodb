Upgrading to ArangoDB 1.2 {#Upgrading12}
========================================

@NAVIGATE_Upgrading12
@EMBEDTOC{Upgrading12TOC}

Upgrading {#Upgrading12Introduction}
====================================

ArangoDB 1.2 provides a lot of new features and APIs when compared to ArangoDB
1.1.

In addition, a few of the existing APIs have changed in ArangoDB 1.2 to exploit
some of the new features and to make using ArangoDB even easier.  This causes
some backwards-incompatibilities but is unavoidable to make the new features
available to end users.

The following list contains changes in ArangoDB 1.2 that are not 100%
downwards-compatible to ArangoDB 1.1 (and partly, to ArangoDB 1.0).

Existing users of ArangoDB 1.1 should read the list carefully and make sure they
have undertaken all necessary steps and precautions before upgrading from
ArangoDB 1.1 to ArangoDB 1.2. Please also check @ref Upgrading12Troubleshooting.

32 Bit Systems
--------------

As the GCC uses a different padding for C structure under 32bit systems compared
to 64bit systems, the 1.1 datafiles were not interchangeable. 1.2 fixes this
problem.

There is however one caveat: If you already installed 1.2.beta1 on a 32bit
system, the upgrade will no longer worker. In this case you need to export the
data first, upgrade ArangoDB, and import the data again. There is no problem, if
you upgrade from 1.1 to 1.2.beta2.


Database Directory Version Check and Upgrade {#Upgrading12VersionCheck}
-----------------------------------------------------------------------

Starting with ArangoDB 1.1, _arangod_ will perform a database version check at
startup. This has not changed in ArangoDB 1.2.

The version check will look for a file named _VERSION_ in its database
directory. If the file is not present, _arangod_ in version 1.2 will perform an
auto-upgrade (can also be considered a database "initialisation"). This
auto-upgrade will create the system collections necessary to run ArangoDB, and
it will also create the VERSION file with a version number of `1.2` inside.

If the _VERSION_ file is present but is from a non-matching version of ArangoDB
(e.g. ArangoDB 1.1 if you upgrade), _arangod_ will refuse to start. Instead, it
will ask you start the server with the option `--upgrade`.  Using the
`--upgrade` option will make the server trigger any required upgrade tasks to
migrate data from ArangoDB 1.1 to ArangoDB 1.2.

This manual invocation of an upgrade shall ensure that users have full control
over when they perform any updates/upgrades of their data, and do not risk
running an incompatible tandem of server and database versions.

If you try starting an ArangoDB 1.2 server with a database created by an earlier
version of ArangoDB, and did not invoke the upgrade procedure, the output of
ArangoDB will look like this:

    > bin/arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/11

    ...
    2013-02-06T14:44:35Z [4399] ERROR Database directory version (1.1) is lower than server version (1.3).
    2013-02-06T14:44:35Z [4399] ERROR It seems like you have upgraded the ArangoDB binary. If this is what you wanted to do, please restart with the --upgrade option to upgrade the data in the database directory.
    2013-02-06T14:44:35Z [4399] FATAL Database version check failed. Please start the server with the --upgrade option
    ...

So it is really necessary to forcefully invoke the upgrade procedure. Please
create a backup of your database directory before upgrading. Please also make
sure that the database directory and all subdirectories and files in it are
writeable for the user you run ArangoDB with.

As mentioned, invoking the upgrade procedure can be done by specifying the
additional command line option `--upgrade` as follows:

    > bin/arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/11 --upgrade

    ...
    2013-02-06T14:47:36Z [4482] INFO Starting upgrade from version 1.1 to 1.2.beta1
    2013-02-06T14:47:36Z [4482] INFO Found 12 defined task(s), 3 task(s) to run
    2013-02-06T14:47:36Z [4482] INFO Executing task #1 (upgradeMarkers12): update markers in all collection datafiles
    2013-02-06T14:48:18Z [4482] INFO Task successful
    2013-02-06T14:48:18Z [4482] INFO Executing task #2 (setupStructures): setup _structures collection
    2013-02-06T14:48:18Z [4482] INFO Task successful
    2013-02-06T14:48:18Z [4482] INFO Executing task #3 (createStructuresIndex): create index on collection attribute in _structures collection
    2013-02-06T14:48:19Z [4482] INFO Task successful
    2013-02-06T14:48:19Z [4482] INFO Upgrade successfully finished
    ...

The upgrade procecure will execute the defined tasks to run _arangod_ with all
new features and data formats. It should normally run without problems and
indicate success at its end. If it detects a problem that it cannot fix, it will
halt on the first error and warn you.

Re-starting arangod with the `--upgrade` option will execute only the previously
failed and not yet executed tasks.

Upgrading from ArangoDB 1.1 to ArangoDB 1.2 will modify some header data inside
the collection datafiles. The upgrade procedure will create a backup of all
collection files it processes, in case something goes wrong.

In case the upgrade did not produce any problems and ArangoDB works well for you
after the upgrade, you may want to remove these backup files manually.

All collection datafiles will be backed up in the original collection
directories in the database directory. You can easily detect the backup files
because they have a filename ending in `.old`.

Please note that the upgrade behavior in 1.2 was slightly changed compared to
1.1:

- in 1.1, when starting `arangod` with `--upgrade`, the server performed the
  upgrade, and if the upgrade was successfully, continued to work in either
  daemon or console mode.

- in 1.2, when started with the `--upgrade` option, the server will perfom
  the upgrade, and then exit with a status code indicating the result of the
  upgrade (0 = success, 1 = failure). To start the server regularly in either 
  daemon or console mode, the `--upgrade` option must not be specified.
  This change was introduced to allow init.d scripts check the result of
  the upgrade procedure, even in case an upgrade was successful.

Upgrade a binary package {#UpgradingBinaryPackage}
---------------------------------------------------

Linux:
- Upgrade ArangoDB by package manager (Example `zypper update arangodb`)
- check configuration file: `/etc/arangodb/arangod.conf`
- Upgrade database files with `/etc/init.d/arangodb upgrade`

Mac OS X binary package
- You can find the new Mac OS X packages here: `http://www.arangodb.org/repositories/MacOSX`
- check configuration file: `/etc/arangodb/arangod.conf`
- Upgrade database files `/usr/sbin/arangod --upgrade'

Mac OS X with homebrew
- Upgrade ArangoDB by `brew upgrade arangodb'
- check configuration file: `/usr/local/Cellar/1.X.Y/etc/arangodb/arangod.conf`
- Upgrade database files `/usr/local/sbin/arangod --upgrade`

Troubleshooting{#Upgrading12Troubleshooting}
============================================

Problem: ArangoDB fails on startup with "directory not writable"
----------------------------------------------------------------

ArangoDB 1.2 will check on startup whether the database directory 
and all collection directories in it are writeable for the user
ArangoDB is started with. This check has been added to prevent errors
caused by ArangoDB not having permissions to write to its own database
directory.

When the server is started and ArangoDB detects that one of the directories
is not writeable, it will abort with a message as follows:

    ...
    2013-02-06T15:10:09Z [5034] ERROR database directory '/tmp/migrate' is not writable for current user
    2013-02-06T15:07:46Z [4990] FATAL cannot open database '/tmp/migrate'
    ...

or

    ...
    2013-02-06T15:07:46Z [4990] ERROR database subdirectory '/tmp/migrate/collection-33374320' is not writable for current user
    2013-02-06T15:07:46Z [4990] FATAL cannot open database '/tmp/migrate'
    ...

If this happens, please make sure the user that ArangoDB is run with
has all necessary privileges (especially write privileges) for the database
directory, the collection subdirectories and files in it.


Problem: How to create a new ArangoDB user?
-------------------------------------------

In ArangoDB 1.0 and 1.1, ArangoDB provided a separate shell script named 
"arango-password" to create new users for ArangoDB. Using this shell script required
exclusive access to the database so this was no proper solution.

Starting with ArangoDB 1.2, users can be managed from ArangoDB itself. 

For example, `arangosh` can be used to add, update, and change users. Example:

    arangosh> var users = require("org/arangodb/users");
    arangosh> users.save("myuser@example.com", "examplepassword");
    { "error" : false, "_id" : "_users/316955178143", "_rev" : "316955178143", "_key" : "316955178143" }

To update the password of an existing user:

    arangosh> var users = require("org/arangodb/users");
    arangosh> users.replace("myuser@example.com", "newpassword");
    { "error" : false, "_id" : "_users/316955178143", "_rev" : "316955505823", "_key" : "316955178143" }

To remove an existing user:

    arangosh> users.remove("myuser@example.com");
    true

Please note that any modifications to user data will be saved by ArangoDB in
the `_users` system collection. The contents of this collection are cached.
To flush the cache and reload the user data from this collection, you should
run the following command in addition:
    
    arangosh> users.reload();
    true

Please make sure that in case you specify a password in arangosh, you do
not save the arangosh command history.  

If the server is run with authentication turned on, `arangosh` can only
connect to the server if username and password are specified when `arangosh`
is started. If you do not have a username or a password of an existing user,
you can use the ArangoDB emergency console to setup a new user. This should
be considered the last resort, as the `arangod` emergency console requires
exclusive access to the database, and cannot be used over a network connection.

To invoke the emergency console to manage users, do the following:

    > bin/arangod --console /tmp/voctest

    ArangoDB JavaScript emergency console [V8 version 3.9.4, DB version 1.3.alpha]
    arangod> var users = require("org/arangodb/users");
    arangod> users.save("WillNever", "ForgetMyPasswordAgain");
    { "_id" : "_users/316955911976", "_rev" : "316955911976", "_key" : "316955911976" }

Please also refer to @ref Upgrading11Troubleshooting.

API changes in ArangoDB 1.2 {#Upgrading12APIChanges}
====================================================

Changed format for document handles (`_id`)
-------------------------------------------

ArangoDB 1.2 uses a modified format for the value returned in the `_id` attribute of
a document. While ArangoDB 1.1 and earlier generated the `_id` value as a combination of
server-generated collection id and document id, ArangoDB 1.2 will return a combination
of collection name and user-defined document key.

A document returned by ArangoDB 1.1 and earlier looked like this:

    { "_id" : "1234/6789", ... }

with `1234` being the collection id, and `6789` being the document id.

In ArangoDB 1.2, the same document would look like this (provided the name of the
collection is "mycollection"):
    
    { "_id" : "mycollection/6789", ... }

It is possible for users to define their own value for the `_key` attribute of a
document upon document creation. In case the document was created with a `_key`
value of `mytest`:
    
    { "_id" : "mycollection/mytest", ... }

`_key` attribute returned for documents
---------------------------------------

In ArangoDB 1.2, all documents returned by the server will have an extra `_key`
attribute. This attribute was not present in ArangoDB 1.1. The value of the `_key`
attribute is a unique string (unique only in the context of the collection the
document is contained in).

The value for `_key` is a server-generated value, or a user-generated value if
the user provides `_key` values on document creation.

Document revision id (`_rev`) returned as string
------------------------------------------------

Document revision ids created and returned by ArangoDB 1.2 are now returned 
as strings with the numeric revision id value inside. 
ArangoDB 1.0 and ArangoDB 1.1 returned revision ids as simple numeric values
without encapsulating them in a string. The revision id is part of every
document stored in ArangoDB and is returned by ArangoDB in the `_rev` attribute.
  
A document in ArangoDB 1.1 will be returned as follows:

    { "_rev": 1234, ... } 

whereas a document in ArangoDB 1.2 will be returned as follows:

    { "_rev": "1234", ... } 

The change was determined to be necessary to prevent potential value overrun/truncation 
in any part of the client/server workflow. The integer values returned by previous
versions of ArangoDB may not work for clients that do not provide an arbitrary, or at
least 64 bit, precision integer type. Clients that store big integers values in IEEE754
doubles are subject to precision loss if the values get too big.

The change affects both the internal Javascript APIs and the public REST APIs, 
primarily @ref RestDocument and @ref RestEdge.

Clients that are strictly typed may need to be adjusted to work with this change.

Collection id returned as string
--------------------------------

The same change as for the document revision ids has been carried out for collection
ids in ArangoDB 1.2. That means that collection ids in ArangoDB 1.2 will be returned
as numeric values encapsulated in strings.

A collection object from ArangoDB 1.1 has looked like this:
    
    { "id": 9327643, "name": "test", ... }
  
A collection object returned by ArangoDB 1.2 looks like this:

    { "id": "9327643", "name": "test", ... }

This change affects the internal Javascript APIs that deal with collection ids, and
all public REST APIs that return the collection id, mostly @ref HttpCollection).

Clients that are strictly typed may need to be adjusted to work with this change.

Cursor ids returned as strings
------------------------------

Cursor ids returned by ArangoDB are also returned as numeric values encapsulated
in strings starting from ArangoDB 1.2. As with the other types, they have been
returned as simple integers numbers in ArangoDB 1.1 and before.

A cursor object returned by ArangoDB 1.0 / 1.1 has looked like this:

    { "id": 11734292, "hasMore": true, ... } 

whereas a cursor object returned by ArangoDB 1.2 looks like this:

    { "id": "11734292", "hasMore": true, ... } 

This change affects the AQL query and cursor cursor management REST API 
(@ref HttpCursorHttp).

Clients that are strictly typed may need to be adjusted to work with this change.

Index ids returned as strings
-----------------------------

Index ids are also returned as numeric values encapsulated in strings starting with
ArangoDB 1.2. They have been returned as simple integers in ArangoDB 1.1 and before.

This change affects the internal Javascript APIs that deal with index ids, and the
public REST index API (@ref HttpIndex).

Clients that are strictly typed may need to be adjusted to work with this change.

Additional return attributes
----------------------------

A few REST API functions return additional attributes in ArangoDB 1.2:
- `GET /_api/collection/<collection-name>/figures` returns an extra attribute `shapes`
- `GET /_api/collection/<collection-name>/figures` returns an extra attribute `isVolatile`
- `GET /_api/collection/<collection-name>/properties` returns an extra attribute `isVolatile`
- `GET /_api/collection/<collection-name>/count` returns an extra attribute `isVolatile`

Clients that are strictly typed may need to be adjusted to work with this change.

Changed error return codes
--------------------------

When an unknown collection was used in an AQL query, ArangoDB 1.1 and before
returned the error code `1520` (```Unable to open collection```). ArangoDB 1.2
will instead return the standard error code `1203` (```Collection not found```),
which is also returned by other parts of ArangoDB in case a non-existing collection
is accessed.

When a collection was created with a name that was already in use for another 
collection, ArangoDB 1.1 returned an error code `1207` (```Duplicate name```) with
an HTTP status code of `400` (```Bad request```). For this case, the HTTP status
code has been changed to HTTP `409` (```Conflict```) in ArangoDB 1.2.

Changes in available global variables and functions
===================================================

Some variables, functions, and objects have been removed from the global scope in 
ArangoDB 1.2, and have been moved into namespaced modules. 

This change primarily affects usage of console features in `arangosh` and `arangod`, 
but may also affect custom user actions carried out by ArangoDB when used as an 
application server.

Most of the functions previously available in the global scope have been moved to
the `internal` namespace, which can be made available from both `arangosh` and 
`arangod` as follows:

    arangosh> var internal = require("internal");

After that, there will be variable named `internal` which can be used as previously
in ArangoDB 1.1.

For convenience, `arangosh` in ArangoDB 1.2 still provides the global object `db`
to access all collections easily. However, the global `edges` variable is gone in 
ArangoDB 1.2.
Instead of using the `edges` object to create or access a collection, use the `db`
object in ArangoDB 1.2:

    arangosh> db._create("myCollection");
    arangosh> db._createEdgeCollection("myEdgeCollection");
    arangosh> db.myEdgeCollection.save(...);

In `arangod`, either in daemon mode or when running the emergency console, the
following global variables are missing in ArangoDB 1.2:

- `db`: can be made available by ```var db = require("org/arangodb").db;```
- `edges`: not needed in ArangoDB 1.2. Please use `db`'s functionality instead.
- `internal`: can be made available via ```var internal = require("internal");```

File Layout {#Upgrading12ConfigChanges}
=======================================

The file locations for readline console history files have been unified in 
ArangoDB 1.2 as follows:
- the history for `arangod`'s emergency console is now stored in file
  `$HOME/.arangod`. It was stored in `$HOME/.arango` before.
- the history for `arangosh` is still stored in file `$HOME/.arangosh` as before.
- the history for `arangoirb` is now stored in file `$HOME/.arangoirb`. It was
  stored in `$HOME/.arango-mrb` before.

Removed Features {#Upgrading12RemovedFeatures}
==============================================

Removed Functionality {#Upgrading12RemovedFunctionality}
--------------------------------------------------------

### arangosh

The global `edges` variable has been removed in `arangosh`. In ArangoDB 1.1 and
before this variable could be used to create and access edge collections.

Since ArangoDB 1.1, all collections can be accessed using the `db` variable, and
there was no need to keep the `edges` variable any longer.

### arangoimp

The parameter `--reuse-ids` for _arangoimp_ was removed. This parameter was a
workaround in ArangoDB 1.1 to re-import documents with a specific document id.
This is not necessary in ArangoDB 1.2 as this version provides user-defined
key values which can and should be used instead.

The separator parameter `--separator` for _arangoimp_ was changed in 1.2 to
allow exactly one separator character. In previous versions, separators longer
than one character were supported.

The parameter `--eol` for _arangoimp_ was also removed in 1.2. 
Line endings are now detected automatically by _arangoimp_ as long as the 
standard line endings CRLF (Windows) or LF (Unix) are used.

### arango-password

In ArangoDB 1.0 and 1.1 ArangoDB was shipped with a shell script named 
"arango-password" to create new users for ArangoDB. This has been removed in
ArangoDB 1.2 in favor of managing users via `arangosh` or `arangod` directly.

Please refer to @ref Upgrading12Troubleshooting for more information on how
to manage users in ArangoDB 1.2.

