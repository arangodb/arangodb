Incompatible changes in ArangoDB 3.4
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.4, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.4:

Geo indexes
-----------

- The on-disk storage format for indexes of type `geo` has changed for the RocksDB
  storage engine. This also affects `geo1` and `geo2` indexes.

  This **requires** users to start the arangod process with the
  `--database.auto-upgrade true` option to allow ArangoDB recreating these 
  indexes using the new on-disk format.

  The on-disk format for geo indexes is incompatible with the on-disk format used
  in 3.3 and 3.2, so downgrading from 3.4 to 3.3 is not supported.

- Geo indexes will now be reported no longer as _geo1_ or _geo2_ but as type `geo`. 
  The two previously known geo index types (`geo1`and `geo2`) are **deprecated**. 
  APIs for creating indexes (`ArangoCollection.ensureIndex`) will continue to support 
  `geo1`and `geo2`.


RocksDB engine
--------------

Installations that start using ArangoDB 3.4 will use an optimized on-disk format
for storing documents using the RocksDB storage engine. This format cannot be used
ArangoDB 3.3 or before, meaning it is not possible to downgrade from a fresh 3.4
install to 3.3 or earlier when using the RocksDB engine.

Installations that were originally set up with older versions of ArangoDB (e.g. 3.2
or 3.3) will continue to use the existing on-disk format for the RocksDB engine
even with ArangoDB 3.4.


HTTP REST API
-------------

The following incompatible changes were made in context of ArangoDB's HTTP REST
APIs:

- `GET /_api/index` will now return type `geo` for geo indexes, not type `geo1`
  or `geo2`.

  For geo indexes, the index API will not return the attributes `constraint` and
  `ignoreNull` anymore. These attributes were initially deprecated in ArangoDB 2.5

- `GET /_api/aqlfunction` was migrated to match the general structure of
  ArangoDB replies. It now returns an object with a "result" attribute that
  contains the list of available AQL user functions:

  ```json
  {
    "code": 200,
    "error": false,
    "result": [
      {
        "name": "UnitTests::mytest1",
        "code": "function () { return 1; }",
        "isDeterministic": false
      }
    ]
  }
  ```

  In previous versions, this REST API returned only the list of available
  AQL user functions on the top level of the response.
  Each AQL user function description now also contains the 'isDeterministic' attribute.

- `POST /_api/aqlfunction` now includes an "isNewlyCreated" attribute that indicates
  if a new function was created or if an existing one was replaced (in addition to the
  "code" attribute, which remains 200 for replacement and 201 for creation):

  ```json
  {
    "code": "201",
    "error": false,
    "isNewlyCreated": true
  }
  ```

- `DELETE /_api/aqlfunction` now returns the number of deleted functions:

  ```json
  {
    "code": 200,
    "error": false,
    "deletedCount": 10
  }
  ```

- `GET /_admin/status` now returns the attribute `operationMode` instead of `mode`.
  The previously existing attribute `writeOpsEnabled` is no longer returned and was
  replaced with an attribute `readOnly` with the inverted meaning.

- if authentication is turned on, requests to databases by users with insufficient 
  access rights will be answered with HTTP 401 (forbidden) instead of HTTP 404 (not found).


The following APIs have been added or augmented:

- `POST /_api/document/{collection}` now supports repsert (replace-insert). 
  
  This can be achieved by using the URL parameter `overwrite=true`. When set to 
  `true`, insertion will not fail in case of a primary key conflict, but turn 
  into a replace operation.

  When an insert turns into a replace, the previous version of the document can
  be retrieved by passing the URL parameter `returnOld=true`

- APIs for view management have been added at endpoint `/_api/view`.


The following, partly undocumented REST APIs have been removed in ArangoDB 3.4:

- `GET /_admin/test`
- `GET /_admin/clusterCheckPort`
- `GET /_admin/cluster-test` 
- `GET /_admin/statistics/short`
- `GET /_admin/statistics/long`


AQL
---

- the AQL functions `CALL` and `APPLY` may now throw the errors 1540
(`ERROR_QUERY_FUNCTION_NAME_UNKNOWN`) and 1541 (`ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH`)
instead of error 1582 (`ERROR_QUERY_FUNCTION_NOT_FOUND`) in some situations.

- the `NEAR` AQL function now does not default to a limit of 100 documents 
  any more, but will return all documents if no limit is specified.

- the existing "fulltext-index-optimizer" optimizer rule has been removed 
  because its duty is now handled by the new "replace-function-with-index" rule.

- the behavior of the `fullCount` option for AQL query cursors has changed so that it 
  will only take into account `LIMIT` statements on the top level of the query.

  `LIMIT` statements in subqueries will not have any effect on the `fullCount` results
  any more.

- the `NEAR`, `WITHIN` and `FULLTEXT` AQL functions do not support accessing
  collections dynamically anymore.

  The name of the underlying collection and the name of the index attribute to be
  used have to specified using either collection name identifiers, string literals 
  or bind parameters, but must not be specified using query variables.

  For example, the following AQL queries are ok:
 
      FOR doc IN NEAR(myCollection, 2.5, 3) RETURN doc
      FOR doc IN NEAR(@@collection, 2.5, 3) RETURN doc
      FOR doc IN FULLTEXT("myCollection", "body", "foxx") RETURN doc
      FOR doc IN FULLTEXT(@@collection, @attribute, "foxx") RETURN doc

  Contrary, the following queries will fail to execute with 3.4 because of dynamic
  collection/attribute names used in them:

      FOR name IN ["col1", "col2"] FOR doc IN NEAR(name, 2.5, 3) RETURN doc

      FOR doc IN collection 
        FOR match IN FULLTEXT(PARSE_IDENTIFIER(doc).collection, PARSE_IDENTIFIER(doc).key, "foxx") RETURN doc

- the AQL warning 1577 (collection used in expression) will not occur anymore

  It was used in previous versions of ArangoDB when the name of a collection was
  used in an expression in an AQL query, e.g.

      RETURN c1 + c2

  Due to internal changes in AQL this is not detected anymore in 3.4, so this 
  particular warning will not be raised.

- the undocumented built-in visitor functions for AQL traversals have been removed,
  as they were based on JavaScript implementations:
  
  - `HASATTRIBUTESVISITOR`
  - `PROJECTINGVISITOR`
  - `IDVISITOR`
  - `KEYVISITOR`
  - `COUNTINGVISITOR`        


Startup option changes
----------------------

The arangod, the following startup options have changed:

- the hidden option `--server.maximal-threads` is now obsolete.

  Setting the option will have no effect.

- the option `--server.maximal-queue-size` has been renamed to `--server.queue-size`.

- the default value for the existing startup option `--javascript.gc-interval`
  has been increased from every 1000 to every 2000 requests, and the default value
  for the option `--javascript.gc-frequency` has been increased from 30 to 60 seconds.

  This will make the V8 garbage collection run less often by default than in previous
  versions, reducing CPU load a bit and leaving more V8 contexts available on average.

- the startup option `--cluster.my-local-info` has been removed in favor of persisted 
  server UUIDs.

  The option `--cluster.my-local-info` was deprecated since ArangoDB 3.3.


Permissions
-----------

The behavior of permissions for databases and collections changed:

The new fallback rule for databases for which no access level is explicitly 
specified is now:

* Choose the higher access level of:
  * A wildcard database grant
  * A database grant on the `_system` database

The new fallback rule for collections for which no access level is explicitly 
specified is now:

* Choose the higher access level of:
  * Any wildcard access grant in the same database, or on "*/*"
  * The access level for the current database
  * The access level for the `_system` database


SSLv2
-----

Support for SSLv2 has been removed from arangod and all client tools.

Startup will now be aborted when using SSLv2 for a server endpoint, or when connecting 
with one of the client tools via an SSLv2 connection.

SSLv2 has been disabled in the OpenSSL library by default in recent versions
because of security vulnerabilities inherent in this protocol.

As it is not safe at all to use this protocol, the support for it has also
been stopped in ArangoDB. End users that use SSLv2 for connecting to ArangoDB
should change the protocol from SSLv2 to TLSv12 if possible, by adjusting
the value of the `--ssl.protocol` startup option.


Client tools
------------

The client tool _arangoimp_ has been renamed to _arangoimport_ for consistency.
  
Release packages will still install arangoimp as a symlink to arangoimport, 
so user scripts invoking arangoimp do not need to be changed to work with
ArangoDB 3.4.


Miscellaneous changes
---------------------

For the MMFiles engine, the compactor thread(s) were renamed from 
"Compactor" to "MMFilesCompactor".

This change will be visible only on systems which allow assigning names to
threads.
