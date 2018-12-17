Features and Improvements in ArangoDB 3.4
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.4. ArangoDB 3.4 also contains several bug fixes that are not listed
here.

ArangoSearch
------------

ArangoSearch is a sophisticated, integrated full-text search solution over
a user-defined set of attributes and collections. It is the first type of
view in ArangoDB.

- [ArangoSearch tutorial](https://www.arangodb.com/tutorials/arangosearch/)
- [ArangoSearch overview](../Views/ArangoSearch/README.md)
- [ArangoSearch in AQL](../../AQL/Views/ArangoSearch/index.html)


New geo index implementation
----------------------------

### S2 based geo index

The geo index in ArangoDB has been reimplemented based on [S2 library](http://s2geometry.io/)
functionality. The new geo index allows indexing points, but also indexing of more
complex geographical objects. The new implementation is much faster than the previous one for
the RocksDB engine.

Additionally, several AQL functions have been added to facilitate working with
geographical data: `GEO_POINT`, `GEO_MULTIPOINT`, `GEO_LINESTRING`, `GEO_MULTILINESTRING`,
`GEO_POLYGON` and `GEO_MULTIPOLYGON`. These functions will produce GeoJSON objects.

Additionally there are new geo AQL functions `GEO_CONTAINS`, `GEO_INTERSECTS` and `GEO_EQUALS`
for querying and comparing GeoJSON objects.

### AQL Editor GeoJSON Support

As a feature on top, the web ui embedded AQL editor now supports also displaying all
GeoJSON supported data. 


RocksDB storage engine
----------------------

### RocksDB as default storage engine

The default storage engine in ArangoDB 3.4 is now the RocksDB engine.

Previous versions of ArangoDB used MMFiles as the default storage engine. This
change will have an effect for new ArangoDB installations only, and only if no
storage engine is selected explicitly or the storage engine selected is "auto".
In this case, a new installation will default to the RocksDB storage engine.

Existing ArangoDB installations upgraded to 3.4 from previous versions will
continue to use their previously selected storage engine.

### Optimized binary storage format

The RocksDB storage engine in ArangoDB 3.4 now also uses an optimized binary
format for storing documents. This format allows inserting new documents in
an order that RocksDB prefers. Using the new format will reduce the number
of compactions that RocksDB needs to do for the ArangoDB documents stored,
allowing for better long-term insertion performance.

The new binary format will **only be used for new installations** that start with
ArangoDB 3.4. Existing installations upgraded from previous versions will
continue to use the previous binary format.

Note that there is no need to use the new binary format for installations upgraded
from 3.3, as the old binary format will continue to work as before.
In order to use the new binary format with existing data, it is required to
create a logical dump of the database data, shut down the server, erase the
database directory and restore the data from the logical dump. To minimize
downtime you can alternatively run a second arangod instance in your system,
that replicates the original data; once the replication has reached completion,
you can switch the instances.

### Better control of RocksDB WAL sync interval

ArangoDB 3.4 also provides a new configuration option `--rocksdb.sync-interval`
to control how frequently ArangoDB will automatically synchronize data in RocksDB's
write-ahead log (WAL) files to disk. Automatic syncs will only be performed for
not-yet synchronized data, and only for operations that have been executed without
the *waitForSync* attribute.

Automatic synchronization of RocksDB WAL file data is performed by a background
thread in ArangoDB. The default sync interval is 100 milliseconds. This can be
adjusted so syncs happen more or less frequently.

### Reduced replication catch-up time

The catch-up time for comparing the contents of two collections (or shards) on two
different hosts via the incremental replication protocol has been reduced when using
the RocksDB storage engine.

### Improved RocksDB geo index performance

The rewritten geo index implementation 3.4 speeds up the RocksDB-based geo index
functionality by a factor of 3 to 6 for many common cases when compared to the
RocksDB-based geo index in 3.3.

A notable implementation detail of previous versions of ArangoDB was that accessing
a RocksDB collection with a geo index acquired a collection-level lock. This severely
limited concurrent access to RocksDB collections with geo indexes in previous
versions. This requirement is now gone and no extra locks need to be acquired when
accessing a RocksDB collection with a geo index.

### Optional caching for documents and primary index values

The RocksDB engine now provides a new per-collection property `cacheEnabled` which
enables in-memory caching of documents and primary index entries. This can potentially
speed up point-lookups significantly, especially if collection have a subset of frequently
accessed documents.

The option can be enabled for a collection as follows:
```
db.<collection>.properties({ cacheEnabled: true });
```

If the cache is enabled, it will be consulted when reading documents and primary index
entries for the collection. If there is a cache miss and the document or primary index
entry has to be looked up from the RocksDB storage engine, the cache will be populated.

The per-collection cache utilization for primary index entries can be checked via the
command `db.<collection>.indexes(true)`, which will provide the attributes `cacheInUse`,
`cacheSize` and `cacheLifeTimeHitRate`.

Memory for the documents and primary index entries cache will be provided by ArangoDB's
central cache facility, whose maximal size can be configured by adjusting the value of
the startup option `--cache.size`.

Please note that caching may adversely affect the performance for collections that are
frequently updated. This is because cache entries need to be invalidated whenever documents
in the collection are updated, replaced or removed. Additionally, enabling caching will
subtract memory from the overall cache, so that less RAM may be available for other
items that use in-memory caching (e.g. edge index entries). It is therefore recommended
to turn on caching only for dedicated collections for which the caching effects have been
confirmed to be positive.

### Exclusive collection access option

In contrast to the MMFiles engine, the RocksDB engine does not require collection-level
locks. This is good in general because it allows concurrent access to a RocksDB
collection.

Reading documents does not require any locks with the RocksDB engine, and writing documents
will acquire per-document locks. This means that different documents can be modified
concurrently by different transactions.

When concurrent transactions modify the same documents in a RocksDB collection, there
will be a write-write conflict, and one of the transactions will be aborted. This is
incompatible with the MMFiles engine, in which write-write conflicts are impossible due
to its collection-level locks. In the MMFiles engine, a write transaction always has
exclusive access to a collection, and locks out all other writers.

While making access to a collection exclusive is almost always undesired from the
throughput perspective, it can greatly simplify client application development. Therefore
the RocksDB engine now provides optional exclusive access to collections on a
per-query/per-transaction basis.

For AQL queries, all data-modification operations now support the `exclusive` option, e.g.

    FOR doc IN collection
      UPDATE doc WITH { updated: true } IN collection OPTIONS { exclusive: true }

JavaScript-based transactions can specify which collections to lock exclusively in the
`exclusive` sub-attribute of their `collections` attribute:

```js
db._executeTransaction({
  collections: {
    exclusive: [ "collection" ]
  },
  ...
});
```

Note that using exclusive access for RocksDB collections will serialize write operations
to RocksDB collections, so it should be used with extreme care.


### RocksDB library upgrade

The version of the bundled RocksDB library was upgraded from 5.6 to 5.16.

The version of the bundled Snappy compression library used by RocksDB was upgraded from
1.1.3 to 1.1.7.


Collection and document operations
----------------------------------

### Repsert operation

The existing functionality for inserting documents got an extra option to turn
an insert into a replace, in case that a document with the specified `_key` value
already exists. This type of operation is called a "Repsert" (Replace-insert).

Using the new option client applications do not need to check first whether a
given document exists, but can use a single atomic operation to conditionally insert
or replace it.

Here is an example of control flow that was previously necessary to conditionally
insert or replace a document:

```js
doc = { _key: "someKey", value1: 123, value2: "abc" };

// check if the document already exists...
if (!db.collection.exists(doc._key)) {
  // ... document did not exist, so insert it
  db.collection.insert(doc);
} else {
  // ... document did exist, so replace it
  db.collection.replace(doc._key, doc);
}
```

With ArangoDB 3.4 this can now be simplified to:

```js
doc = { _key: "someKey", value1: 123, value2: "abc" };

// insert the document if it does not exist yet, other replace
db.collection.insert(doc, { overwrite: true });
```

Client applications can also optionally retrieve the old revision of the document
in case the insert turned into a replace operation:

```js
doc = { _key: "someKey", value1: 123, value2: "abc" };

// insert the document if it does not exist yet, other replace
// in case of a replace, previous will be populated, in case of an
// insert, previous will be undefined
previous = db.collection.insert(doc, { overwrite: true, returnOld: true }).old;
```

The same functionality is available for the document insert method in the
HTTP REST API. The HTTP endpoint for `POST /_api/document` will now accept the
optional URL parameters `overwrite` and `returnOld`.

AQL also supports making an INSERT a conditional REPSERT. In contrast to regular
INSERT it supports returning the OLD and the NEW document on disk to i.e. inspect
the revision or the previous content of the document.
AQL INSERT is switched to  REPSERT by setting the option `overwrite` for it:

```
INSERT {
 _key: "someKey",
 value1: 123,
 value2: "abc"
} INTO collection OPTIONS { overwrite: true }
RETURN OLD
```

Please note that in a cluster setup the Repsert operation requires the collection
to be sharded by `_key`.


### Graph API extensions

The REST APIs for modifying graphs at endpoint `/_api/gharial` now support returning
the old revision of vertices / edges after modifying them. The APIs also supports
returning the just-inserted vertex / edge. This is in line with the already existing
single-document functionality provided at endpoint `/_api/document`.

The old/new revisions can be accessed by passing the URL parameters `returnOld` and
`returnNew` to the following endpoints:

* /_api/gharial/&lt;graph>/vertex/&lt;collection>
* /_api/gharial/&lt;graph>/edge/&lt;collection>

The exception from this is that the HTTP DELETE verb for these APIs does not
support `returnOld` because that would make the existing API incompatible.

### Additional key generators

In addition to the existing key generators `traditional` (which is still the
default key generator) and `autoincrement`, ArangoDB 3.4 adds the following key
generators:

* `padded`:
  The `padded` key generator generates keys of a fixed length (16 bytes) in
  ascending lexicographical sort order. This is ideal for usage with the RocksDB
  engine, which will slightly benefit keys that are inserted in lexicographically
  ascending order. The key generator can be used in a single-server or cluster.

* `uuid`: the `uuid` key generator generates universally unique 128 bit keys, which
  are stored in hexadecimal human-readable format. This key generator can be used
  in a single-server or cluster to generate "seemingly random" keys. The keys
  produced by this key generator are not lexicographically sorted.

Generators may be chosen with the creation of collections; here an example for
the *padded* key generator:
```
db._create("padded", { keyOptions: { type: "padded" } });

db.padded.insert({});
{
  "_id" : "padded/0000000009d0d1c0",
  "_key" : "0000000009d0d1c0",
  "_rev" : "_XI6VqNK--_"
}

db.padded.insert({});
{
  "_id" : "padded/0000000009d0d1c4",
  "_key" : "0000000009d0d1c4",
  "_rev" : "_XI6VquC--_"
}
```

Example for the *uuid* key generator:
```js
db._create("uuid", { keyOptions: { type: "uuid" } });

db.uuid.insert({});

{
  "_id" : "uuid/16d5dc96-79d6-4803-b547-5a34ce795099",
  "_key" : "16d5dc96-79d6-4803-b547-5a34ce795099",
  "_rev" : "_XI6VPc2--_"
}

db.uuid.insert({});
{
  "_id" : "uuid/0af83d4a-56d4-4553-a97d-c7ed2644dc09",
  "_key" : "0af83d4a-56d4-4553-a97d-c7ed2644dc09",
  "_rev" : "_XI6VQgO--_"
}
```

### Miscellaneous improvements

The command `db.<collection>.indexes()` was added as an alias for the already existing
`db.<collection>.getIndexes()` method for retrieving all indexes of a collection. The
alias name is more consistent with the already existing method names for retrieving
all databases and collections.


Cluster improvements
--------------------

### Load-balancer support

ArangoDB now supports running multiple coordinators behind a load balancer that
randomly routes client requests to the different coordinators. It is not required
anymore that load balancers implement session or connection stickiness on behalf
of ArangoDB.

In particular, the following ArangoDB APIs were extended to work well with load
balancing:

* the cursor API at endpoint `/_api/cursor`
* the jobs API at endpoint `/_api/job`
* the tasks API at endpoint `/_api/tasks`
* Pregel APIs at endpoint `/_api/pregel`

Some of these APIs build up coordinator-local state in memory when being first
accessed, and allow accessing further data using follow-up requests. This caused
problems in previous versions of ArangoDB, when load balancers routed the follow
up requests to these APIs to different coordinators that did not have access to
the other coordinator's in-memory state.

With ArangoDB 3.4, if such an API is accessed by a follow-up request that refers
to state being created on a different coordinator, the actually accessed coordinator
will forward the client request to the correct coordinator. Client applications
and load balancers do not need to be aware of which coordinator they had used
for the previous requests, though from a performance point of view accessing the
same coordinator for a sequence of requests will still be beneficial.

If a coordinator forwards a request to a different coordinator, it will send the
client an extra HTTP header `x-arango-request-forwarded-to` with the id of the
coordinator it forwarded the request to. Client applications or load balancers
can optionally use that information to make follow-up requests to the "correct"
coordinator to save the forwarding.

### Refusal to start mixed-engine clusters

Starting a cluster with coordinators and DB servers using different storage
engines is not supported. Doing it anyway will now log an error and abort a
coordinator's startup.

Previous versions of ArangoDB did not detect the usage of different storage
engines in a cluster, but the runtime behavior of the cluster was undefined.

### Advertised endpoints

It is now possible to configure the endpoints advertised by the
coordinators to clients to be different from the endpoints which are
used for cluster internal communication. This is important for client
drivers which refresh the list of endpoints during the lifetime of the
cluster (which they should do!). In this way one can make the cluster
advertise a load balancer or a separate set of IP addresses for external
access. The new option is called `--cluster.my-advertised-endpoint`.

### Startup safety checks

The new option `--cluster.require-persisted-id` can be used to prevent the startup
of a cluster node using the wrong data directory.

If the option is set to true, then the ArangoDB instance will only start if a
UUID file (containing the instance's cluster-wide ID) is found in the database
directory on startup. Setting this option will make sure the instance is started
using an already existing database directory and not a new one.

For the first start, the UUID file must either be created manually or the option
must be set to `false` for the initial startup and later be changed to `true`.

### Coordinator storage engine

In previous versions of ArangoDB, cluster coordinator nodes used the storage
engine selected by the database administrator (i.e. MMFiles or RocksDB).
Although all database and document data was forwarded from coordinators to be
stored on the database servers and not on the coordinator nodes, the storage
engine used on the coordinator was checking and initializing its on-disk state
on startup.
Especially because no "real" data was stored by the coordinator's storage engine,
using a storage engine here did not provide any value but only introduced
unnecessary potential points of failure.

As of ArangoDB 3.4, cluster coordinator nodes will now use an internal "cluster"
storage engine, which actually does not store any data. That prevents 3.4
coordinators from creating any files or directories inside the database directory
except the meta data files such as `ENGINE`, `LOCK`, `SERVER`, `UUID` and `VERSION`.
And as no files need to be read on coordinator startup except these mentioned
files, it also reduces the possibility of data corruption on coordinator nodes.

### `DBSERVER` role as alias of `PRIMARY`

When starting a _DBServer_, the value `DBSERVER` can now be specified (as alias of
`PRIMARY`) in the option `--cluster.my-role`. The value `PRIMARY` is still accepted.

All REST APIs that currently return "PRIMARY" as _role_, will continue to return
"PRIMARY".


AQL
---

### AQL query profiling

AQL queries can now be executed with optional profiling, using ArangoDB 3.4's new
`db._queryProfile()` function.

This new function is a hybrid of the already existing `db._query()` and `db._explain()`
functions:

* `db._query()` will execute an AQL query, but not show the execution plan nor
  runtime profile information
* `db._explain()` will show the query's execution plan, but not execute the query
* `db._queryProfile()` will run the query, collect the runtime costs of each component
  of the query, and finally show the query's execution plan with actual runtime information.
  This is very useful for debugging AQL query performance and optimizing queries.

For more information please refer to the [Query Profiling](../../AQL/ExecutionAndPerformance/QueryProfiler.html)
page.

### Revised cluster-internal AQL protocol

When running an AQL query in a cluster, the coordinator has to distribute the
individual parts of the AQL query to the relevant shards that will participate
in the execution of the query.

Up to including ArangoDB 3.3, the coordinator has deployed the query parts to the
individual shards one by one. The more shards were involved in a query, the more
cluster-internal requests this required, and the longer the setup took.

In ArangoDB 3.4 the coordinator will now only send a single request to each of
the involved database servers (in contrast to one request per shard involved).
This will speed up the setup phase of most AQL queries, which will be noticable for
queries that affect a lot of shards.

The AQL setup has been changed from a two-step protocol to a single-step protocol,
which additionally reduces the total number of cluster-internal requests necessary
for running an AQL query.

The internal protocol and APIs have been adjusted so that AQL queries can now get
away with less cluster-internal requests than in 3.3 also after the setup phase.

Finally, there is now an extra optimization for trivial AQL queries that will only
access a single document by its primary key (see below).

### AQL functions added

The following AQL functions have been added in ArangoDB 3.4:

* `TO_BASE64`: creates the base64-encoded representation of a value
* `TO_HEX`: creates a hex-encoded string representation of a value
* `ENCODE_URI_COMPONENT`: URI-encodes a string value, for later usage in URLs
* `SOUNDEX`: calculates the soundex fingerprint of a string value
* `ASSERT`: aborts a query if a condition is not met
* `WARN`: makes a query produce a warning if a condition is not met
* `IS_KEY`: this function checks if the value passed to it can be used as a document
  key, i.e. as the value of the `_key` attribute for a document
* `SORTED`: will return a sorted version of the input array using AQL's internal
  comparison order
* `SORTED_UNIQUE`: same as `SORTED`, but additionally removes duplicates
* `COUNT_DISTINCT`: counts the number of distinct / unique items in an array
* `LEVENSHTEIN_DISTANCE`: calculates the Levenshtein distance between two string values
* `REGEX_MATCHES`: finds matches in a string using a regular expression
* `REGEX_SPLIT`: splits a string using a regular expression
* `UUID`: generates a universally unique identifier value
* `TOKENS`: splits a string into tokens using a language-specific text analyzer
* `VERSION`: returns the server version as a string
 
The following AQL functions have been added to make working with geographical 
data easier:

* `GEO_POINT`
* `GEO_MULTIPOINT`
* `GEO_POLYGON`
* `GEO_LINESTRING`
* `GEO_MULTILINESTRING`
* `GEO_CONTAINS`
* `GEO_INTERSECTS`
* `GEO_EQUALS`.

The first five functions will produce GeoJSON objects from coordinate data. The
latter three functions can be used for querying and comparing GeoJSON objects.

The following AQL functions can now be used as aggregation functions in a
COLLECT statement:

* `UNIQUE`
* `SORTED_UNIQUE`
* `COUNT_DISTINCT`

The following function aliases have been created for existing AQL functions:

* `CONTAINS_ARRAY` is an alias for `POSITION`
* `KEYS` is an alias for `ATTRIBUTES`

### Distributed COLLECT

In the general case, AQL COLLECT operations are expensive to execute in a cluster,
because the database servers need to send all shard-local data to the coordinator
for a centralized aggregation.

The AQL query optimizer can push some parts of certain COLLECT operations to the
database servers so they can do a per-shard aggregation. The database servers can
then send only the already aggregated results to the coordinator for a final aggregation.
For several queries this will reduce the amount of data that has to be transferred
between the database servers servers and the coordinator by a great extent, and thus
will speed up these queries. Work on this has started with ArangoDB 3.3.5, but
ArangoDB 3.4 allows more cases in which COLLECT operations can partially be pushed to
the database servers.

In ArangoDB 3.3, the following aggregation functions could make use of a distributed
COLLECT in addition to `COLLECT WITH COUNT INTO` and `RETURN DISTINCT`:

* `COUNT`
* `SUM`
* `MIN`
* `MAX`

ArangoDB 3.4 additionally enables distributed COLLECT queries that use the following
aggregation functions:

* `AVERAGE`
* `VARIANCE`
* `VARIANCE_SAMPLE`
* `STDDEV`
* `STDDEV_SAMPLE`

### Native AQL function implementations

All built-in AQL functions now have a native implementation in C++.
Previous versions of ArangoDB had AQL function implementations in both C++ and
in JavaScript.

The JavaScript implementations of AQL functions were powered by the V8 JavaScript
engine, which first required the conversion of all function input into V8's own
data structures, and a later conversion of the function result data into ArangoDB's
native format.

As all AQL functions are now exclusively implemented in native C++, no more
conversions have to be performed to invoke any of the built-in AQL functions.
This will considerably speed up the following AQL functions and any AQL expression
that uses any of these functions:

* `APPLY`
* `CALL`
* `CURRENT_USER`
* `DATE_ADD`
* `DATE_COMPARE`
* `DATE_DAYOFWEEK`
* `DATE_DAYOFYEAR`
* `DATE_DAYS_IN_MONTH`
* `DATE_DAY`
* `DATE_DIFF`
* `DATE_FORMAT`
* `DATE_HOUR`
* `DATE_ISO8601`
* `DATE_ISOWEEK`
* `DATE_LEAPYEAR`
* `DATE_MILLISECOND`
* `DATE_MINUTE`
* `DATE_MONTH`
* `DATE_NOW`
* `DATE_QUARTER`
* `DATE_SECOND`
* `DATE_SUBTRACT`
* `DATE_TIMESTAMP`
* `DATE_YEAR`
* `IS_DATESTRING`
* `IS_IN_POLYGON`
* `LTRIM`
* `RTRIM`
* `FIND_FIRST`
* `FIND_LAST`
* `REVERSE`
* `SPLIT`
* `SUBSTITUTE`
* `SHA512`
* `TRANSLATE`
* `WITHIN_RECTANGLE`

Additionally, the AQL functions `FULLTEXT`, `NEAR` and `WITHIN` now use the native
implementations even when executed in a cluster. In previous versions of ArangoDB,
these functions had native implementations for single-server setups only, but fell
back to using the JavaScript variants in a cluster environment.

Apart from saving conversion overhead, another side effect of adding native
implementations for all built-in AQL functions is, that AQL does not require the usage
of V8 anymore, except for user-defined functions.

If no user-defined functions are used in AQL, end users do not need to put aside
dedicated V8 contexts for executing AQL queries with ArangoDB 3.4, making server
configuration less complex and easier to understand.

### AQL optimizer query planning improvements
 
The AQL query optimizer will by default now create at most 128 different execution
plans per AQL query. In previous versions the maximum number of plans was 192.

Normally the AQL query optimizer will generate a single execution plan per AQL query, 
but there are some cases in which it creates multiple competing plans. More plans
can lead to better optimized queries, however, plan creation has its costs. The
more plans are created and shipped through the optimization pipeline, the more
time will be spent in the optimizer.
To make the optimizer better cope with some edge cases, the maximum number of plans
created is now strictly enforced and was lowered compared to previous versions of
ArangoDB. This helps a specific class of complex queries.

Note that the default maximum value can be adjusted globally by setting the startup 
option `--query.optimizer-max-plans` or on a per-query basis by setting a query's
`maxNumberOfPlans` option.

### Condition simplification

The query optimizer rule `simplify-conditions` has been added to simplify certain
expressions inside CalculationNodes, which can speed up runtime evaluation of these
expressions.

The optimizer rule `fuse-filters` has been added to merge adjacent FILTER conditions
into a single FILTER condition where possible, allowing to save some runtime registers.

### Single document optimizations

In a cluster, the cost of setting up a distributed query can be considerable for
trivial AQL queries that will only access a single document, e.g.

    FOR doc IN collection FILTER doc._key == ... RETURN doc
    FOR doc IN collection FILTER doc._key == ... RETURN 1

    FOR doc IN collection FILTER doc._key == ... REMOVE doc IN collection
    FOR doc IN collection FILTER doc._key == ... REMOVE doc._key IN collection
    REMOVE... IN collection

    FOR doc IN collection FILTER doc._key == ... UPDATE doc WITH { ... } IN collection
    FOR doc IN collection FILTER doc._key == ... UPDATE doc._key WITH { ... } IN collection
    UPDATE ... WITH { ... } IN collection

    FOR doc IN collection FILTER doc._key == ... REPLACE doc WITH { ... } IN collection
    FOR doc IN collection FILTER doc._key == ... REPLACE doc._key WITH { ... } IN collection
    REPLACE ... WITH { ... } IN collection

    INSERT { ... } INTO collection

All of the above queries will affect at most a single document, identified by its
primary key. The AQL query optimizer can now detect this, and use a specialized
code path for directly carrying out the operation on the participating database
server(s). This special code path bypasses the general AQL query cluster setup and
shutdown, which would have prohibitive costs for these kinds of queries.

In case the optimizer makes use of the special code path, the explain output will
contain a node of the type `SingleRemoteOperationNode`, and the optimizer rules
will contain `optimize-cluster-single-document-operations`.

The optimization will fire automatically only for queries with the above patterns.
It will only fire when using `_key` to identify a single document,
and will be most effective if `_key` is also used as the collection's shard key.

### Subquery optimizations

The AQL query optimizer can now optimize certain subqueries automatically so that
they perform less work.

The new optimizer rule `optimize-subqueries` will fire in the following situations:

* in case only a few results are used from a non-modifying subquery, the rule will
  automatically add a LIMIT statement into the subquery.

  For example, the unbounded subquery

      LET docs = (
        FOR doc IN collection
          FILTER ...
          RETURN doc
      )
      RETURN docs[0]

  will be turned into a subquery that only produces a single result value:

      LET docs = (
        FOR doc IN collection
          FILTER ...
          LIMIT 1
          RETURN doc
      )
      RETURN docs[0]

* in case the result returned by a subquery is not used later but only the number
  of subquery results, the optimizer will modify the result value of the subquery
  so that it will return constant values instead of potentially more expensive
  data structures.

  For example, the following subquery returning entire documents

        RETURN LENGTH(
          FOR doc IN collection
            FILTER ...
            RETURN doc
        )

    will be turned into a subquery that returns only simple boolean values:

        RETURN LENGTH(
          FOR doc IN collection
            FILTER ...
            RETURN true
        )

  This saves fetching the document data from disk in first place, and copying it
  from the subquery to the outer scope.
  There may be more follow-up optimizations.

### COLLECT INTO ... KEEP optimization

When using an AQL COLLECT ... INTO without a *KEEP* clause, then the AQL query
optimizer will now automatically detect which sub-attributes of the *INTO* variables 
are used later in the query. The optimizer will add automatic *KEEP* clauses to
the COLLECT statement then if possible.
    
For example, the query
    
    FOR doc1 IN collection1
      FOR doc2 IN collection2
	COLLECT x = doc1.x INTO g
	RETURN { x, all: g[*].doc1.y }
    
will automatically be turned into
    
    FOR doc1 IN collection1
      FOR doc2 IN collection2
	COLLECT x = doc1.x INTO g KEEP doc1
	RETURN { x, all: g[*].doc1.y }
   
This prevents variable `doc2` from being temporarily stored in the variable `g`,
which saves processing time and memory, especially for big result sets.

### Fullcount changes

The behavior of the `fullCount` option for AQL query cursors was adjusted to conform
to users' demands. The value returned in the `fullCount` result attribute will now
be produced only by the last `LIMIT` statement on the upper most level of the query -
hence `LIMIT` statements in subqueries will not have any effect on the
`fullCount` results any more.

This is a change to previous versions of ArangoDB, in which the `fullCount`
value was produced by the sequential last `LIMIT` statement in a query,
regardless if the `LIMIT` was on the top level of the query or in a subquery.

The `fullCount` result value will now also be returned for queries that are served
from the query results cache.

### Relaxed restrictions for LIMIT values

The `offset` and `count` values used in an AQL LIMIT clause can now be expressions, as
long as the expressions can be resolved at query compile time.
For example, the following query will now work:

    FOR doc IN collection
      LIMIT 0, CEIL(@percent * @count / 100) 
      RETURN doc

Previous versions of ArangoDB required the `offset` and `count` values to be
either number literals or numeric bind parameter values.

### Improved sparse index support

The AQL query optimizer can now use sparse indexes in more cases than it was able to
in ArangoDB 3.3. If a sparse index is not used in a query because the query optimizer
cannot prove itself that the index attribute value cannot be `null`, it is now often
useful to add an extra filter condition to the query that requires the sparse index'
attribute to be non-null.

For example, if for the following query there is a sparse index on `value` in any
of the collections, the optimizer cannot prove that `value` can never be `null`:

    FOR doc1 IN collection1
      FOR doc2 IN collection2
        FILTER doc1.value == doc2.value
        RETURN [doc1, doc2]

By adding an extra filter condition to the query that excludes `null` values explicitly,
the optimizer in 3.4 will now be able to use a sparse index on `value`:

    FOR doc1 IN collection1
      FOR doc2 IN collection2
        FILTER doc1.value == doc2.value
        FILTER doc2.value != null
        RETURN [doc1, doc2]

The optimizer in 3.3 was not able to detect this, and refused to use sparse indexes
for such queries.

### Query results cache

The AQL query results cache in ArangoDB 3.4 has got additional parameters to 
control which queries should be stored in the cache.

In addition to the already existing configuration option `--query.cache-entries`
that controls the maximum number of query results cached in each database's
query results cache, there now exist the following extra options:

- `--query.cache-entries-max-size`: maximum cumulated size of the results stored
  in each database's query results cache
- `--query.cache-entry-max-size`: maximum size for an individual cache result
- `--query.cache-include-system-collections`: whether or not results of queries
  that involve system collections should be stored in the query results cache

These options allow more effective control of the amount of memory used by the
query results cache, and can be used to better utilitize the cache memory.

The cache configuration can be changed at runtime using the `properties` function
of the cache. For example, to limit the per-database number of cache entries to
256 MB and to limit the per-database cumulated size of query results to 64 MB, 
and the maximum size of each individual cache entry to 1MB, the following call
could be used:

```
require("@arangodb/aql/cache").properties({
  maxResults: 256,
  maxResultsSize: 64 * 1024 * 1024,
  maxEntrySize: 1024 * 1024,
  includeSystem: false
});
```

The contents of the query results cache can now also be inspected at runtime using 
the cache's new `toArray` function:

```
require("@arangodb/aql/cache").toArray();
```

This will show all query results currently stored in the query results cache of
the current database, along with their query strings, sizes, number of results
and original query run times.

The functionality is also available via HTTP REST APIs.


### Miscellaneous changes

When creating query execution plans for a query, the query optimizer was fetching
the number of documents of the underlying collections in case multiple query
execution plans were generated. The optimizer used these counts as part of its 
internal decisions and execution plan costs calculations. 

Fetching the number of documents of a collection can have measurable overhead in a
cluster, so ArangoDB 3.4 now caches the "number of documents" that are referred to
when creating query execution plans. This may save a few roundtrips in case the
same collections are frequently accessed using AQL queries. 

The "number of documents" value was not and is not supposed to be 100% accurate 
in this stage, as it is used for rough cost estimates only. It is possible however
that when explaining an execution plan, the "number of documents" estimated for
a collection is using a cached stale value, and that the estimates change slightly
over time even if the underlying collection is not modified.


Streaming AQL Cursors
---------------------

AQL query cursors created by client applications traditionally executed an AQL query,
and built up the entire query result in memory. Once the query completed, the results
were sent back to the client application in chunks of configurable size.

This approach was a good fit for the MMFiles engine with its collection-level locks,
and usually smaller-than-RAM query results. For the RocksDB engine with its document-level
locks and lock-free reads and potentially huge query results, this approach does not always
fit.

ArangoDB 3.4 allows to optionally execute AQL queries initiated via the cursor API in a
streaming fashion. The query result will then be calculated on the fly, and results are
sent back to the client application as soon as they become available on the server, even
if the query has not yet completed.

This is especially useful for queries that produce big result sets (e.g.
`FOR doc IN collection RETURN doc` for big collections). Such queries will take very long
to complete without streaming, because the entire query result will be computed first and
stored in memory. Executing such queries in non-streaming fashion may lead to client
applications timing out before receiving the first chunk of data from the server. Additionally,
creating a huge query result set on the server may make it run out of memory, which is also
undesired. Creating a streaming cursor for such queries will solve both problems.

Please note that streaming cursors will use resources all the time till you
fetch the last chunk of results.

Depending on the storage engine used this has different consequences:

- **MMFiles**: While before collection locks would only be held during the creation of the cursor
  (the first request) and thus until the result set was well prepared,
  they will now be held until the last chunk requested
  by the client through the cursor is processed.

  While Multiple reads are possible, one write operation will effectively stop
  all other actions from happening on the collections in question.
- **RocksDB**: Reading occurs on the state of the data when the query
  was started. Writing however will happen during working with the cursor.
  Thus be prepared for possible conflicts if you have other writes on the collections,
  and probably overrule them by `ignoreErrors: True`, else the query
  will abort by the time the conflict happenes.

Taking into account the above consequences, you shouldn't use streaming
cursors light-minded for data modification queries.

Please note that the query options `cache`, `count` and `fullCount` will not work with streaming
cursors. Additionally, the query statistics, warnings and profiling data will only be available
when the last result batch for the query is sent. Using a streaming cursor will also prevent
the query results being stored in the AQL query results cache.

By default, query cursors created via the cursor API are non-streaming in ArangoDB 3.4,
but streaming can be enabled on a per-query basis by setting the `stream` attribute
in the request to the cursor API at endpoint `/_api/cursor`.

However, streaming cursors are enabled automatically for the following parts of ArangoDB in 3.4:

* when exporting data from collections using the arangoexport binary
* when using `db.<collection>.toArray()` from the Arango shell

Please note that AQL queries consumed in a streaming fashion have their own, adjustable
"slow query" threshold. That means the "slow query" threshold can be configured seperately for 
regular queries and streaming queries.

Native implementations
----------------------

The following internal and user-facing functionality has been ported from 
JavaScript-based implementations to C++-based implementations in ArangoDB 3.4:

* the statistics gathering background thread
* the REST APIs for
  - managing user defined AQL functions
  - graph management  at `/_api/gharial` that also does:
    - vertex management
    - edge management
* the implementations of all built-in AQL functions
* all other parts of AQL except user-defined functions
* database creation and setup
* all the DBserver internal maintenance tasks for shard creation, index
  creation and the like in the cluster

By making the listed functionality not use and not depend on the V8 JavaScript 
engine, the respective functionality can now be invoked more efficiently in the
server, without requiring the conversion of data between ArangoDB's native format 
and V8's internal formats. For the maintenance operations this will lead to
improved stability in the cluster.

As a consequence, ArangoDB agency and database server nodes in an ArangoDB 3.4 
cluster will now turn off the V8 JavaScript engine at startup entirely and automatically.
The V8 engine will still be enabled on cluster coordinators, single servers and
active failover instances. But even the latter instance types will not require as 
many V8 contexts as previous versions of ArangoDB.
This should reduce problems with servers running out of available V8 contexts or
using a lot of memory just for keeping V8 contexts around.


Foxx
----

The functions `uuidv4` and `genRandomBytes` have been added to the `crypto` module.

The functions `hexSlice`, `hexWrite` have been added to the `Buffer` object.

The functions `Buffer.from`, `Buffer.of`, `Buffer.alloc` and `Buffer.allocUnsafe`
have been added to the `Buffer` object for improved compatibility with node.js.


Security
--------

### Ownership for cursors, jobs and tasks

Cursors for AQL query results created by the API at endpoint `/_api/cursor` 
are now tied to the user that first created the cursor.

Follow-up requests to consume or remove data of an already created cursor will
now be denied if attempted by a different user.

The same mechanism is also in place for the following APIs:

- jobs created via the endpoint `/_api/job`
- tasks created via the endpoint `/_api/tasks`


### Dropped support for SSLv2

ArangoDB 3.4 will not start when attempting to bind the server to a Secure Sockets
Layer (SSL) v2 endpoint. Additionally, the client tools (arangosh, arangoimport,
arangodump, arangorestore etc.) will refuse to connect to an SSLv2-enabled server.

SSLv2 can be considered unsafe nowadays and as such has been disabled in the OpenSSL
library by default in recent versions. ArangoDB is following this step.

Clients that use SSLv2 with ArangoDB should change the protocol from SSLv2 to TLSv12
if possible, by adjusting the value of the `--ssl.protocol` startup option for the
`arangod` server and all client tools.


Distribution Packages
---------------------

In addition to the OS-specific packages (eg. _rpm_ for Red Hat / CentOS, _deb_ for
Debian, NSIS installer for Windows etc.) starting from 3.4.0 new `tar.gz` archive packages
are available for Linux and Mac. They correspond to the `.zip` packages for Windows,
which can be used for portable installations, and to easily run different ArangoDB
versions on the same machine (e.g. for testing).


Client tools
------------

### Arangodump

Arangodump can now dump multiple collections in parallel. This can significantly
reduce the time required to take a backup.

By default, arangodump will use 2 threads for dumping collections. The number of
threads used by arangodump can be adjusted by using the `--threads` option when
invoking it.

### Arangorestore

Arangorestore can now restore multiple collections in parallel. This can significantly
reduce the time required to recover data from a backup.

By default, arangorestore will use 2 threads for restoring collections. The number of
threads used by arangorestore can be adjusted by using the `--threads` option when
invoking it.

### Arangoimport

Arangoimp was renamed to arangoimport for consistency.
The 3.4 release packages will still install `arangoimp` as a symlink so user scripts
invoking `arangoimp` do not need to be changed.

[Arangoimport now can pace the data load rate automatically](../Programs/Arangoimport/Details.md#automatic-pacing-with-busy-or-low-throughput-disk-subsystems)
based on the actual rate of
data the server can handle. This is useful in contexts when the server has a limited
I/O bandwidth, which is often the case in cloud environments. Loading data too quickly
may lead to the server exceeding its provisioned I/O operations quickly, which will
make the cloud environment throttle the disk performance and slowing it down drastically.
Using a controlled and adaptive import rate allows preventing this throttling.

The pacing algorithm is turned on by default, but can be disabled by manually specifying
any value for the `--batch-size` parameter.

Arangoimport also got an extra option `--create-database` so that it can automatically
create the target database should this be desired. Previous versions of arangoimp
provided options for creating the target collection only
(`--create-collection`, `--create-collection-type`).

Finally, arangoimport got an option `--latency` which can be used to print microsecond
latency statistics on 10 second intervals for import runs. This can be used to get
additional information about the import run performance and performance development.


Miscellaneous features
----------------------

### Logging without escaping non-printable characters

The new option `--log.escape` can be used to enable a slightly different log output
format.

If set to `true` (which is the default value), then the logging will work as in
previous versions of ArangoDB, and the following characters in the log output are
escaped:

* the carriage return character (hex 0d)
* the newline character (hex 0a)
* the tabstop character (hex 09)
* any other characters with an ordinal value less than hex 20

If the `--log.escape` option is set to `false` however, no characters are escaped
when logging them. Characters with an ordinal value less than hex 20 (including
carriage return, newline and tabstop) will not be printed in this mode, but will
be replaced with a space character (hex 20). This is because these characters are
often undesired in logs anyway.
Another positive side effect of turning off the escaping is that it will slightly
reduce the CPU overhead for logging. However, this will only be noticable when the
logging is set to a very verbose level (e.g. log levels debug or trace).


### Active Failover

The _Active Failover_ mode is now officially supported for multiple slaves.

Additionally you can now send read-only requests to followers, so you can
use them for read scaling. To make sure only requests that are intended for
this use-case are served by the follower you need to add a
`X-Arango-Allow-Dirty-Read: true` header to HTTP requests.

For more information see
[Active Failover Architecture](../Architecture/DeploymentModes/ActiveFailover/Architecture.md).
