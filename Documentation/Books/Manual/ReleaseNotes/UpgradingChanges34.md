Incompatible changes in ArangoDB 3.4
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.4, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.4:


Supported platforms
-------------------

The minimum supported architecture for the official release packages of ArangoDB is
now the Nehalem architecture.

All release packages are built with compiler optimizations that require at least
this architecture. The following CPU features are required for running an official
release package (note: these are all included in the Nehalem architecture and upwards):

* SSE2
* SSE3
* SSE4.1
* SSE4.2

In case the target platform does not conform to these requirements, ArangoDB may
not work correctly.

The compiled-in architecture optimizations can be retrieved on most platforms by 
invoking the *arangod* binary with the `--version` option. The optimization switches
will then show up in the output in the line starting with `optimization-flags`, e.g.

```
$ arangod --version
...
optimization-flags: -march=nehalem -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mno-sse4a -mno-avx -mno-fma -mno-bmi2 -mno-avx2 -mno-xop -mno-fma4 -mno-avx512f -mno-avx512vl -mno-avx512pf -mno-avx512er -mno-avx512cd -mno-avx512dq -mno-avx512bw -mno-avx512ifma -mno-avx512vbmi
platform: linux
```

Note that to get even more target-specific optimizations, it is possible for end
users to compile ArangoDB on their own with compiler optimizations tailored to the
target environment.


Storage engine
--------------

In ArangoDB 3.4, the default storage engine for new installations is the RocksDB
engine. This differs to previous versions (3.2 and 3.3), in which the default
storage engine was the MMFiles engine.

The MMFiles engine can still be explicitly selected as the storage engine for
all new installations. It's only that the "auto" setting for selecting the storage
engine will now use the RocksDB engine instead of MMFiles engine.

In the following scenarios, the effectively selected storage engine for new
installations will be RocksDB:

* `--server.storage-engine rocksdb`
* `--server.storage-engine auto`
* `--server.storage-engine` option not specified

The MMFiles storage engine will be selected for new installations only when 
explicitly selected:

* `--server.storage-engine mmfiles`


To make users aware of that the RocksDB storage engine was chosen automatically
due to an explicit other storage engine selection, 3.4 will come up with the following
startup warning:

    using default storage engine 'rocksdb', as no storage engine was explicitly selected via the `--server.storage-engine` option.
    please note that default storage engine has changed from 'mmfiles' to 'rocksdb' in ArangoDB 3.4


On upgrade, any existing ArangoDB installation will keep its previously selected
storage engine. The change of the default storage engine in 3.4 is thus only relevant
for new ArangoDB installations and/or existing cluster setups for which new server 
nodes get added later. All server nodes in a cluster setup should use the same
storage engine to work reliably. Using different storage engines in a cluster is
unsupported.

To validate that the different nodes in a cluster deployment use the same storage
engine throughout the entire cluster, there is now a startup check performed by
each coordinator. Each coordinator will contact all DB servers and check if the
same engine on the DB server is the same as its local storage engine. In case 
there is any discrepancy, the coordinator will abort its startup.


Geo indexes
-----------

- The on-disk storage format for indexes of type `geo` has changed for the RocksDB
  storage engine. This also affects `geo1` and `geo2` indexes.

  This **requires** users to start the arangod process with the
  `--database.auto-upgrade true` option to allow ArangoDB recreating these 
  indexes using the new on-disk format.

  The on-disk format for geo indexes is incompatible with the on-disk format used
  in 3.3 and 3.2, so an in-place downgrade from 3.4 to 3.3 is not supported.

- Geo indexes will now be reported no longer as _geo1_ or _geo2_ but as type `geo`. 
  The two previously known geo index types (`geo1`and `geo2`) are **deprecated**. 
  APIs for creating indexes (`ArangoCollection.ensureIndex`) will continue to support 
  `geo1`and `geo2`.


RocksDB engine data storage format
----------------------------------

Installations that start using ArangoDB 3.4 will use an optimized on-disk format
for storing documents using the RocksDB storage engine. This format cannot be used
on ArangoDB 3.3 or before, meaning it is not possible to perform an in-place downgrade
from a fresh 3.4 install to 3.3 or earlier when using the RocksDB engine. For more
information on how to downgrade, please refer to the [Downgrading](../Downgrading/README.md)
chapter.

Installations that were originally set up with older versions of ArangoDB (e.g. 3.2
or 3.3) will continue to use the existing on-disk format for the RocksDB engine
even with ArangoDB 3.4 (unless you install a fresh 3.4 package and restore a backup
of your data on this fresh installation).

In order to use the new binary format with existing data, it is required to 
create a logical dump of the database data, shut down the server, erase the 
database directory and restore the data from the logical dump. To minimize 
downtime you can alternatively run a second arangod instance in your system,
that replicates the original data; once the replication has reached completion, 
you can switch the instances.


Threading and request handling
------------------------------

The processing of incoming requests and the execution of requests by server threads
has changed in 3.4.

Previous ArangoDB versions had a hard-coded implicit lower bound of 64 running 
threads, and up to which they would increase the number of running server threads.
That value could be increased further by adjusting the option `--server.maximal-threads`.
The configuration option `--server.threads` existed, but did not effectively set
or limit the number of running threads.

In ArangoDB 3.4, the number of threads ArangoDB uses for request handling can now 
be strictly bounded by configuration options.

The number of server threads is now configured by the following startup options:

- `--server.minimal-threads`: determines the minimum number of request processing
  threads the server will start and always keep around
- `--server.maximal-threads`: determines the maximum number of request processing 
  threads the server will start for request handling. If that number of threads is 
  already running, arangod will not start further threads for request handling

The actual number of request processing threads is adjusted dynamically at runtime
and will float between `--server.minimal-threads` and `--server.maximal-threads`.


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

- APIs for view management have been added at endpoint `/_api/view`.

- The REST APIs for modifying graphs at endpoint `/_api/gharial` now support returning
  the old revision of vertices / edges after modifying them. The APIs also supports 
  returning the just-inserted vertex / edge. This is in line with the already existing 
  single-document functionality provided at endpoint `/_api/document`.

  The old/new revisions can be accessed by passing the URL parameters `returnOld` and
  `returnNew` to the following endpoints:

  * /_api/gharial/<graph>/vertex/<collection>
  * /_api/gharial/<graph>/edge/<collection>

  The exception from this is that the HTTP DELETE verb for these APIs does not
  support `returnOld` because that would make the existing API incompatible.

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

  Additionally, using collections in arbitrary AQL expressions as above is unsupported
  in a mixed cluster that is running a 3.3 coordinator and 3.4 DB server(s). The
  DB server(s) running 3.4 will in this case not be able to use a collection in an
  arbitrary expression, and instead throw an error.

- the undocumented built-in visitor functions for AQL traversals have been removed,
  as they were based on JavaScript implementations:
  
  - `HASATTRIBUTESVISITOR`
  - `PROJECTINGVISITOR`
  - `IDVISITOR`
  - `KEYVISITOR`
  - `COUNTINGVISITOR`        

  Using any of these functions from inside AQL will now produce an error.


Usage of V8
-----------

The internal usage of the V8 JavaScript for non-user actions has been reduced 
in ArangoDB. Several APIs have been rewritten to not depend on V8 and thus do 
not require using a V8 context for execution.

Compared to ArangoDB 3.3, the following parts of ArangoDB can now be used 
without requiring V8 contexts:

- all of AQL (with the exception of user-defined functions)
- the graph modification APIs at endpoint `/_api/gharial`
- background server statistics gathering

Reduced usage of V8 by ArangoDB may allow end users to lower the configured 
numbers of V8 contexts to start. In terms of configuration options, these
are:

- `--javascript.v8-contexts`: the maximum number of V8 contexts to create
- `--javascript.v8-contexts-minimum`: the minimum number of V8 contexts to 
  create at server start and to keep around

The default values for these startup options have not been changed in ArangoDB
3.4, but depending on the actual workload, 3.4 ArangoDB instances may need
less V8 contexts than 3.3.


Startup option changes
----------------------

For arangod, the following startup options have changed:

- the number of server threads is now configured by the following startup options:

  - `--server.minimal-threads`: determines the minimum number of request processing
    threads the server will start
  - `--server.maximal-threads`: determines the maximum number of request processing 
    threads the server will start

  The actual number of request processing threads is adjusted dynamically at runtime
  and will float between `--server.minimal-threads` and `--server.maximal-threads`. 

- the default value for the existing startup option `--javascript.gc-interval`
  has been increased from every 1000 to every 2000 requests, and the default value
  for the option `--javascript.gc-frequency` has been increased from 30 to 60 seconds.

  This will make the V8 garbage collection run less often by default than in previous
  versions, reducing CPU load a bit and leaving more V8 contexts available on average.

- the startup option `--cluster.my-local-info` has been removed in favor of persisted 
  server UUIDs.

  The option `--cluster.my-local-info` was deprecated since ArangoDB 3.3.

- the startup option `--database.check-30-revisions` was removed. It was used for
  checking the revision ids of documents for having been created with ArangoDB 3.0,
  which required a dump & restore migration of the data to 3.1.

  As direct upgrades from ArangoDB 3.0 to 3.4 or from 3.1 to 3.4 are not supported,
  this option has been removed in 3.4.

- the startup option `--server.session-timeout` has been obsoleted. Setting this 
  option will not have any effect.

- the option `--replication.automatic-failover` was renamed to `--replication.active-failover`

  Using the old option name will still work in ArangoDB 3.4, but support for the old 
  option name will be removed in future versions of ArangoDB.


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


Mixed-engine clusters
---------------------

Starting a cluster with coordinators and DB servers using different storage 
engines is not supported. Doing it anyway will now log an error and abort a 
coordinator's startup.

Previous versions of ArangoDB did not detect the usage of different storage
engines in a cluster, but the runtime behavior of the cluster was undefined.


Client tools
------------

The client tool _arangoimp_ has been renamed to _arangoimport_ for consistency.
  
Release packages will still install arangoimp as a symlink to arangoimport, 
so user scripts invoking arangoimp do not need to be changed to work with
ArangoDB 3.4.


Miscellaneous changes
---------------------

For the MMFiles engine, the compactor thread(s) were renamed from "Compactor" 
to "MMFilesCompactor".

This change will be visible only on systems which allow assigning names to
threads.
