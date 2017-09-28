Features and Improvements
=========================

The following list shows in detail which features have been added or improved in
ArangoDB 3.2. ArangoDB 3.2 also contains several bugfixes that are not listed
here.


Storage engines
---------------

ArangoDB 3.2 offers two storage engines:

* the always-existing memory-mapped files storage engine
* a new storage engine based on [RocksDB](https://www.github.com/facebook/rocksdb/)

### Memory-mapped files storage engine (MMFiles)

The former storage engine (named MMFiles engine henceforth) persists data in memory-mapped 
files. 

Any data changes are done first in the engine's write-ahead log (WAL). The WAL 
is replayed after a crash so the engine offers durability and crash-safety. Data
from the WAL is eventually moved to collection-specific datafiles. The files are
always written in an append-only fashion, so data in files is never overwritten.
Obsolete data in files will eventually be purged by background compaction threads.

Most of this engine's indexes are built in RAM. When a collection is loaded, this requires 
rebuilding the indexes in RAM from the data stored on disk. The MMFiles engine has 
collection-level locking.

This storage engine is a good choice when data (including the indexes) can fit in the 
server's available RAM. If the size of data plus the in-memory indexes exceeds the size
of the available RAM, then this engine may try to allocate more memory than available.
This will either make the operating system swap out parts of the data (and cause disk I/O)
or, when no swap space is configured, invoke the operating system's out-of-memory process 
killer.

The locking strategy allows parallel reads and is often good enough in read-mostly
workloads. Writes need exclusive locks on the collections, so they can block other
operations in the same collection. The locking strategy also provides transactional consistency
and isolation.

### RocksDB storage engine

The RocksDB storage engine is new in ArangoDB 3.2. It is designed to store datasets
that are bigger than the server's available RAM. It persists all data (including the
indexes) in a RocksDB instance.

That means any document read or write operations will be answered by RocksDB under the
hood. RocksDB will serve the data from its own in-RAM caches or from disk.
The RocksDB engine has a write-ahead log (WAL) and uses background threads for compaction.
It supports data compression.

The RocksDB storage engine has document-level locking. Read operations do not block and
are never blocked by other operations. Write operations only block writes on the same 
documents/index values. Because multiple writers can operate in parallel on the same
collection, there is the possibility of write-write conflicts. If such write conflict
is detected, one of the write operations is aborted with error 1200 ("conflict").
Client applications can then either abort the operation or retry, based on the required
consistency semantics.

### Storage engine selection

The storage engine to use in an ArangoDB cluster or a single-server instance must be
selected initially. The default storage engine in ArangoDB 3.2 is the MMFiles engine if
no storage engine is selected explicitly. This ensures all users upgrading from earlier
versions can continue with the well-known MMFiles engine.

To select the storage-engine, there is the configuration option `--server.storage-engine`.
It can be set to either `mmfiles`, `rocksdb` or `auto`. While the first two values will
explicitly select a storage engine, the `auto` option will automatically choose the
storage engine based on which storage engine was previously selected. If no engine was
selected previously, `auto` will select the MMFiles engine. If an engine was previously
selected, the selection will be written to a file `ENGINE` in the server's database
directory and will be read from there at any subsequent server starts.

Once the storage engine was selected, the selection cannot be changed by adjusting
`--server.storage-engine`. In order to switch to another storage engine, it is required
to re-start the server with another (empty) database directory. In order to use data
created with the other storage engine, it is required to dump the data first with the
old engine and restore it using the new storage engine. This can be achieved via 
invoking arangodump and arangorestore.

Unlike in MySQL, the storage engine selection in ArangoDB is for an entire cluster or
an entire single-server instance. All databases and collections will use the same storage 
engine. 

### RocksDB storage engine: supported index types

The existing indexes in the RocksDB engine are all persistent. The following indexes are
supported there:

* primary: this type of index is automatically created. It indexes `_id` / `_key`

* edge: this index is automatically created for edge collections. It indexes 
  `_from` and `_to`

* hash, skiplist, persistent: these are user-defined indexes, Despite their names, they are 
  neither hash nor skiplist indexes. These index types map to the same RocksDB-based
  sorted index implementation. The same is true for the "persistent" index. The names 
  "hash", "skiplist" and "persistent" are only used for compatibility with the MMFiles 
  engine where these indexes existed in previous and the current version of ArangoDB.

* geo: user-defined index for proximity searches

* fulltext: user-defined sorted reverted index on words occurring in documents

Satellite Collections
---------------------

With SatelliteCollections, you can define collections to shard to a cluster and
collections to replicate to each machine. The ArangoDB query optimizer knows where
each shard is located and sends the requests to the DBServers involved, which then
executes the query, locally. With this approach, network hops during join
operations on sharded collections can be avoided and response times can be close to
that of a single instance.

[Satellite collections](../Administration/Replication/Synchronous/Satellites.md)
are available in the *Enterprise* edition.


Memory management
-----------------

* make arangod start with less V8 JavaScript contexts

  This speeds up the server start and makes arangod use less memory at start.
  Whenever a V8 context is needed by a Foxx action or some other JavaScript operation 
  and there is no usable V8 context, a new context will be created dynamically now.

  Up to `--javascript.v8-contexts` V8 contexts will be created, so this option 
  will change its meaning. Previously as many V8 contexts as specified by this 
  option were created at server start, and the number of V8 contexts did not
  change at runtime. Now up to this number of V8 contexts will be in use at the 
  same time, but the actual number of V8 contexts is dynamic.

  The garbage collector thread will automatically delete unused V8 contexts after 
  a while. The number of spare contexts will go down to as few as configured in 
  the new option `--javascript.v8-contexts-minimum`. Actually that many V8 contexts 
  are also created at server start.

  The first few requests in new V8 contexts may take longer than in contexts 
  that have been there already. Performance may therefore suffer a bit for the
  initial requests sent to ArangoDB or when there are only few but performance-
  critical situations in which new V8 contexts need to be created. If this is a 
  concern, it can easily be fixed by setting `--javascipt.v8-contexts-minimum`
  and `--javascript.v8-contexts` to a relatively high value, which will guarantee
  that many number of V8 contexts to be created at startup and kept around even
  when unused.

  Waiting for an unused V8 context will now also abort and write a log message
  in case no V8 context can be acquired/created after 60 seconds.

* the number of pending operations in arangod can now be limited to a configurable
  number. If this number is exceeded, the server will now respond with HTTP 503
  (service unavailable). The maximum size of pending operations is controlled via
  the startup option `--server.maximal-queue-size`. Setting it to 0 means "no limit".

* the in-memory document revisions cache was removed entirely because it did not
  provide the expected benefits. The 3.1 implementation shadowed document data in 
  RAM, which increased the server's RAM usage but did not speed up document lookups
  too much.
 
  This also obsoletes the startup options `--database.revision-cache-chunk-size` and
  `--database.revision-cache-target-size`.

  The MMFiles engine now does not use a document revisions cache but has in-memory
  indexes and maps documents to RAM automatically via mmap when documents are
  accessed. The RocksDB engine has its own mechanism for caching accessed documents.


Communication Layer
-------------------

* HTTP responses returned by arangod will now include the extra HTTP header 
  `x-content-type-options: nosniff` to work around a cross-site scripting bug
  in MSIE

* the default value for `--ssl.protocol` was changed from TLSv1 to TLSv1.2.
  When not explicitly set, arangod and all client tools will now use TLSv1.2.

* the JSON data in all incoming HTTP requests in now validated for duplicate
  attribute names.

  Incoming JSON data with duplicate attribute names will now be rejected as
  invalid. Previous versions of ArangoDB only validated the uniqueness of
  attribute names inside incoming JSON for some API endpoints, but not
  consistently for all APIs.

* Internal JavaScript REST actions will now hide their stack traces to the client
  unless in HTTP responses. Instead they will always log to the logfile.


JavaScript
----------

* updated V8 version to 5.7.0.0

* change undocumented behaviour in case of invalid revision ids in
  `If-Match` and `If-None-Match` headers from 400 (BAD) to 412 (PRECONDITION
  FAILED).

* change default string truncation length from 80 characters to 256 characters for
  `print`/`printShell` functions in ArangoShell and arangod. This will emit longer
  prefixes of string values before truncating them with `...`, which is helpful
  for debugging. This change is mostly useful when using the ArangoShell (arangosh).


* the `@arangodb` module now provides a `time` function which returns the current time
  in seconds as a floating point value with microsecond precision.


Foxx
----

* There is now an [official HTTP API for managing services](../../HTTP/Foxx/index.html),
  allowing services to be installed, modified, uninstalled and reconfigured without
  the administrative web interface.

* It is now possible to upload a single JavaScript file instead of a zip archive
  if your service requires no configuration, additional files or setup.
  A minimal manifest will be generated automatically upon installation and the
  uploaded file will be used as the service's main entry point.


Distributed Graph Processing
----------------------------

* We added support for executing distributed graph algorithms aka `Pregel`. 
* Users can run arbitrary algorithms on an entire graph, including in cluster mode.
* We implemented a number of algorithms for various well-known graph measures:
  * Connected Components
  * PageRank
  * Shortest Paths
  * Centrality Measures (Centrality and Betweeness)
  * Community Detection (via Label Propagation, Speakers-Listeners Label Propagation or DMID)
* Users can contribute their own algorithms

AQL
---

### Optimizer improvements

* Geo indexes are now implicitly and automatically used when using appropriate SORT/FILTER 
  statements in AQL, without the need to use the somewhat limited special-purpose geo AQL 
  functions `NEAR` or `WITHIN`.
  
  Compared to using the special purpose AQL functions this approach has the
  advantage that it is more composable, and will also honor any `LIMIT` values
  used in the AQL query.

  The special purpose `NEAR` AQL function can now be substituted with the
  following AQL (provided there is a geo index present on the `doc.latitude`
  and `doc.longitude` attributes):

      FOR doc in geoSort 
        SORT DISTANCE(doc.latitude, doc.longitude, 0, 0) 
        LIMIT 5 
        RETURN doc

  `WITHIN` can be substituted with the following AQL:

      FOR doc in geoFilter 
        FILTER DISTANCE(doc.latitude, doc.longitude, 0, 0) < 2000 
        RETURN doc


### Miscellaneous improvements

* added `REGEX_REPLACE` AQL function

  `REGEX_REPLACE(text, search, replacement, caseInsensitive) → string`

  Replace the pattern *search* with the string *replacement* in the string
  *text*, using regular expression matching.

  - **text** (string): the string to search in
  - **search** (string): a regular expression search pattern
  - **replacement** (string): the string to replace the *search* pattern with
  - returns **string** (string): the string *text* with the *search* regex
    pattern replaced with the *replacement* string wherever the pattern exists
    in *text*

* added new startup option `--query.fail-on-warning` to make AQL queries
  abort instead of continuing with warnings. 

  When set to *true*, this will make an AQL query throw an exception and
  abort in case a warning occurs. This option should be used in development to catch
  errors early. If set to *false*, warnings will not be propagated to exceptions and
  will be returned with the query results. The startup option can also be overriden
  on a per query-level.

* the slow query list now contains the values of bind variables used in the
  slow queries. Bind variables are also provided for the currently running 
  queries. This helps debugging slow or blocking queries that use dynamic
  collection names via bind parameters.

* AQL breaking change in cluster:
  The SHORTEST_PATH statement using edge collection names instead
  of a graph names now requires to explicitly name the vertex collection names 
  within the AQL query in the cluster. It can be done by adding `WITH <name>`
  at the beginning of the query.
  
  Example:
  ```
  FOR v,e IN OUTBOUND SHORTEST_PATH @start TO @target edges [...]
  ```

  Now has to be:

  ```
  WITH vertices
  FOR v,e IN OUTBOUND SHORTEST_PATH @start TO @target edges [...]
  ```

  This change is due to avoid deadlock sitations in clustered case.
  An error stating the above is included.


Client tools
------------

* added data export tool, arangoexport.

  arangoexport can be used to export collections to json, jsonl or xml
  and export a graph or collections to xgmml.

* added "jsonl" as input file type for arangoimp

* added `--translate` option for arangoimp to translate attribute names from
  the input files to attriubte names expected by ArangoDB

  The `--translate` option can be specified multiple times (once per translation
  to be executed). The following example renames the "id" column from the input
  file to "_key", and the "from" column to "_from", and the "to" column to "_to":

      arangoimp --type csv --file data.csv --translate "id=_key" --translate "from=_from" --translate "to=_to"

  `--translate` works for CSV and TSV inputs only.

* added `--threads` option to arangoimp to specify the number of parallel import threads 

* changed default value for client tools option `--server.max-packet-size` from 128 MB 
  to 256 MB. this allows transferring bigger result sets from the server without the
  client tools rejecting them as invalid.


Authentication
--------------

* added [LDAP](../Administration/Configuration/Ldap.md) authentication (Enterprise only)


Authorization
--------------

* added read only mode for users
* collection level authorization rights

Read more in the [overview](../Administration/ManagingUsers/README.md).


Foxx
----

* the [cookie session transport](../Foxx/Sessions/Transports/Cookie.md) now supports all options supported by the [cookie method of the response object](../Foxx/Router/Response.md#cookie).

* it's now possible to provide your own version of the `graphql-sync` module when using the [GraphQL extensions for Foxx](../Foxx/GraphQL.md) by passing a copy of the module using the new _graphql_ option.

* custom API endpoints can now be tagged using the [tag method](../Foxx/Router/Endpoints.md#tag) to generate a cleaner Swagger documentation.


Miscellaneous Changes
---------------------

* arangod now validates several OS/environment settings on startup and warns if
  the settings are non-ideal. It additionally will print out ways to remedy the
  options.
  
  Most of the checks are executed on Linux systems only.

* added "deduplicate" attribute for array indexes, which controls whether inserting 
  duplicate index values from the same document into a unique array index will lead to 
  an error or not:

      // with deduplicate = true, which is the default value:
      db._create("test");
      db.test.ensureIndex({ type: "hash", fields: ["tags[*]"], deduplicate: true });
      db.test.insert({ tags: ["a", "b"] });
      db.test.insert({ tags: ["c", "d", "c"] }); // will work, because deduplicate = true
      db.test.insert({ tags: ["a"] }); // will fail
      
      // with deduplicate = false
      db._create("test");
      db.test.ensureIndex({ type: "hash", fields: ["tags[*]"], deduplicate: false });
      db.test.insert({ tags: ["a", "b"] });
      db.test.insert({ tags: ["c", "d", "c"] }); // will not work, because deduplicate = false
      db.test.insert({ tags: ["a"] }); // will fail
