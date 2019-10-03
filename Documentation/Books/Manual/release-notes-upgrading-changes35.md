---
layout: default
description: It is recommended to check the following list of incompatible changes before upgrading to ArangoDB 3.5
---
Incompatible changes in ArangoDB 3.5
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.5, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.5:

ID values in log messages
-------------------------

By default, ArangoDB and its client tools now show a 5 digit unique ID value in
any of their log messages, e.g.

    2019-03-25T21:23:19Z [8144] INFO [cf3f4] ArangoDB (version 3.5.0 enterprise [linux]) is ready for business. Have fun!.

In this message, the `cf3f4` is the message's unique ID value. ArangoDB users can
use this ID to build custom monitoring or alerting based on specific log ID values.

The presence of these ID values in log messages may confuse custom log message filtering 
or routing mechanisms that parse log messages and that rely on the old log message
format.

This can be fixed adjusting any existing log message parsers and making them aware
of the ID values. The ID values are always 5 byte strings, consisting of the characters
`[0-9a-f]`. ID values are placed directly behind the log level (e.g. `INFO`) for
general log messages that do not contain a log topic, and directly behind the log
topic for messages that contain a topic, e.g. 

    2019-03-25T21:23:19Z [8144] INFO [cf3f4] ArangoDB (version 3.5.0 enterprise [linux]) is ready for business. Have fun!.
    2019-03-25T21:23:16Z [8144] INFO {authentication} [3844e] Authentication is turned on (system only), authentication for unix sockets is turned on

Alternatively, the log IDs can be suppressed in all log messages by setting the startup
option `--log.ids false` when starting arangod or any of the client tools.


Startup options
---------------

The hidden startup option `--rocksdb.delayed_write_rate` was renamed to the more
consistent `--rocksdb.delayed-write-rate`. When the old option name is used, the 
arangod startup will be aborted with a descriptive error message.

HTTP REST API
-------------
The following APIs have been added:

- [The new Stream Transaction API](http/transaction.html)
- [The new ArangoSearch Analyzer management API](http/analyzers.html)
- [The management of the new TTL indices](http/indexes-ttl.html); this enhances the existing index-API
- [Query the actual shard a document lives in](http/collection-getting.html#return-responsible-shard-for-a-document)

The following APIs have been expanded:

- The [ArangoSearch management API](http/views-arangosearch.html) has the new `commitIntervalMsec` attribute in all routes
- Indexes can now have user-defined names
- The new "ttl" index type has been added to the [index creation API](http/indexes.html)
- [Collection creation API now provides the `smartJoinAttribute` parameter](data-modeling-collections-collection-methods.html)
- [`filter` foxx-tests](http/foxx-miscellaneous.html) for testing

The following documentation has been enhanced:

- the documentation for [collection creation and fetching its properties](http/collection-creating.html) has been made more precise

Web interface
-------------

### Potentially different sort order of documents

In the list of documents for a collection, the documents will now always be sorted
in lexicographical order of their `_key` values. An exception for keys representing 
quasi-numerical values has been removed when doing the sorting in the web interface.

Therefore a document with a key value "10" will now be displayed before a document
with a key value of "9".

### Removal of index types "hash" and "skiplist" from the web UI (RocksDB engine)

For the RocksDB engine, the selection of index types "hash" and "skiplist" 
has been removed from the web interface when creating new indexes. 

The index types "hash", "skiplist" and "persistent" are just aliases of each other 
when using the RocksDB engine, so there is no need to offer all of them in parallel.

We found that offering the different types of indexes while in fact they were the
same often confused end users. We opted for keeping "persistent" because from the
candidates "hash", "skiplist" and "persistent" only "persistent" is actually a valid
description of the index capabilities/implementation.


AQL
---

3.5 enforces the invalidation of variables in AQL queries after usage of an AQL 
COLLECT statements as documented. The documentation for variable invalidation claims
that

    The COLLECT statement will eliminate all local variables in the current scope. 
    After COLLECT only the variables introduced by COLLECT itself are available.

However, the described behavior was not enforced when a COLLECT was preceded by a
FOR loop that was itself preceded by a COLLECT. In the following query the final
RETURN statement accesses variable `key1` though the variable should have been 
invalidated by the COLLECT directly before it:

    FOR x1 IN 1..2 
      COLLECT key1 = x1 
      FOR x2 IN 1..2 
        COLLECT key2 = x2 
        RETURN [key2, key1] 
  
In previous releases, this query was
parsed ok, but the contents of variable `key1` in the final RETURN statement were
undefined. 
  
This change is about making queries as the above fail with a parse error, as an 
unknown variable `key1` is accessed here, avoiding the undefined behavior. This is 
also in line with what the documentation states about variable invalidation.

HTTP Replication APIs
---------------------

### New parameter for WAL tailing API

Tailing of recent server operations via `/_api/wal/tail` gets a new parameter
`syncerId`, which helps in tracking the WAL tick of each client. If set, this
supersedes the parameter `serverId` for this purpose. The API stays backwards
compatible.


Miscellaneous
-------------

### Index creation

In previous versions of ArangoDB, if one attempted to create an index with a
specified `_id`, and that `_id` was already in use, the server would typically
return the existing index with matching `_id`. This is somewhat unintuitive, as
it would ignore if the rest of the definition did not match. This behavior has
been changed so that the server will now return a duplicate identifier error.

ArangoDB 3.5 also disallows creating indexes on the `_id` sub-attribute of an attribute,
`referredTo._id` or `documents[*]._id`. Previous versions of ArangoDB allowed creating
such indexes, but the indexes were non-functional.
Starting with ArangoDB 3.5 such indexes cannot be created anymore, and any attempts to 
create them will fail.

### Version details output

The attribute key `openssl-version` in the server/client tool version details 
output was renamed to `openssl-version-compile-time`.

This change affects the output produced when starting one of the ArangoDB
executables (e.g. arangod, arangosh) with the `--version` command. It also 
changes the attribute name in the detailed response of the `/_api/version` REST API.

### Overcommit settings

On Linux, ArangoDB will now show a startup warning in case the kernel setting 
`vm.overcommit_memory` is set to a value of 2 and the jemalloc memory allocator 
is in use. This combination does not play well together, and may lead to the 
kernel denying arangod's memory allocation requests in more cases than necessary.

Usage of V8
-----------

`ArangoQueryStreamCursor.id()` used to return a 32 bit number, and will now
return a string as similar places where V8 has representations of ArangoDB IDs.
