---
layout: default
description: ArangoDB v3.5 Release Notes New Features
---
Features and Improvements in ArangoDB 3.5
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.5. ArangoDB 3.5 also contains several bug fixes that are not listed
here.

ArangoSearch
------------

### Configurable Analyzers

Analyzers can split string values into smaller parts and perform additional
processing such as word stemming and case conversion. In ArangoDB 3.4 there
is a fixed set of text analyzers for 12 different languages, which tokenize
strings into case-insensitive word stems using language-dependent rules based
on the chosen locale, without discarding any stop-words (common words which
carry little meaning such as "the"). An additional no-operation analyzer
*identity* is available to keep the input unaltered in its entirety.

In 3.5, analyzers can be customized as well as used independent of
ArangoSearch Views in AQL. It is possible to tokenize strings without
word stemming, remove user-defined stop-words, split by a delimiting
character only, perform case conversion and/or removal of diacritic
characters against the full input without tokenization and more.

See [Analyzers](arangosearch-analyzers.html) for all available options.

### Sorted Index

The index behind an ArangoSearch View can have a primary sort order.
A direction can be specified upon View creation for each uniquely named
attribute (ascending or descending), to enable an optimization for AQL
queries which iterate over a view and sort by one of the attributes.
If the index direction matches the requested `SORT` direction, then
the data can be read in order directly from the index without actual
sort operation.

View definition example:

```json
{
  "links": {
    "coll1": {
      "fields": {
        "text": {
        }
      }
    },
    "coll2": {
      "fields": {
        "text": {
      }
    }
  },
  "primarySort": [
    {
      "field": "text",
      "direction": "desc"
    }
  ]
}
```

AQL query example:

```js
FOR doc IN viewName
  SORT doc.text DESC
  RETURN doc
```

Execution plan **without** a sorted index being used:

```
Execution plan:
 Id   NodeType            Est.   Comment
  1   SingletonNode          1   * ROOT
  2   EnumerateViewNode      1     - FOR doc IN viewName   /* view query */
  3   CalculationNode        1       - LET #1 = doc.`val`   /* attribute expression */
  4   SortNode               1       - SORT #1 DESC   /* sorting strategy: standard */
  5   ReturnNode             1       - RETURN doc
```

Execution plan with the primary sort order of the index being utilized:

```
Execution plan:
 Id   NodeType            Est.   Comment
  1   SingletonNode          1   * ROOT
  2   EnumerateViewNode      1     - FOR doc IN viewName SORT doc.`val` DESC   /* view query */
  5   ReturnNode             1       - RETURN doc
```

Note that the `primarySort` option is immutable: it can not be changed after
View creation. It is therefore not possible to configure it through the Web UI.
The View needs to be created via the HTTP or JavaScript API (arangosh) to set it.

See [Primary Sort Order](arangosearch-views.html#primary-sort-order) of
ArangoSearch Views.

### AQL Integration

Some small new features give more control over ArangoSearch from AQL.

- The scoring functions `BM25()` and `TFIDF()` are not limited to the `SORT`
  operation anymore, but can also be returned as part of the query result:

  ```js
  FOR doc IN viewName
    SEARCH ...
    LET score = BM25(doc)
    SORT score DESC
    RETURN { doc, score }
  ```

- The score can be manipulated to influence the ranking based on
  attribute values and using numeric AQL functions:

  ```js
  FOR movie IN imdbView
    SEARCH PHRASE(movie.title, "Star Wars", "text_en")
    SORT BM25(movie) * LOG(movie.runtime + 1) DESC
    RETURN movie
  ```

- The `SEARCH` operation accepts an options object to restrict the search to
  certain collections indexed by a View:

  ```js
  FOR doc IN viewName
    SEARCH ... OPTIONS { collections: ["coll1", "coll2"] }
    RETURN doc
  ```

- A new function `IN_RANGE()` was added for matching values within defined
  boundaries, to enable easy range searching primarily for numbers and strings.

See:
- [AQL Scoring Functions](aql/functions-arangosearch.html#scoring-functions)
- [AQL Search Options](aql/operations-search.html#search-options)
- [IN_RANGE() function](aql/functions-arangosearch.html#in_range)

AQL
---

### Pruning in Traversals

With `PRUNE` you can stop walking down certain paths early in a graph traversal
to improve its efficiency. This is different to `FILTER`, which would perform
a post-filtering after the actual traversal was carried out already in most
cases. Using `PRUNE`, the traverser will not follow any more edges on the
current path if the pruning condition is met, but will emit the traversal
variables for whatever stopped it.

See: [Graph Traversal Pruning](aql/graphs-traversals.html#pruning)

### SORT-LIMIT optimization

A new SORT-LIMIT optimization has been added. This optimization will be pulled off
by the query optimizer if there is a SORT statement followed by a LIMIT node, and the
overall number of documents to return is relatively small in relation to the total
number of documents to be sorted. In this case, the optimizer will use a size-constrained
heap for keeping only the required number of results in memory, which can drastically
reduce memory usage and, for some queries, also execution time for the sorting.

If the optimization is applied, it will show as "sort-limit" rule in the query execution
plan.

Also see:
- [AQL Optimizer Rules](aql/execution-and-performance-optimizer.html#list-of-optimizer-rules)
  (`sort-limit` rule)
- [Sort-Limit Optimization in AQL](https://www.arangodb.com/2019/03/sort-limit-optimization-aql/){:target="_blank"}
  (blog post)

### Index hints in AQL

Users may now take advantage of the `indexHint` inline query option to override
the internal optimizer decision regarding which index to use to serve content
from a given collection. The index hint works with the [named indices](#named-indices)
feature, making it easy to specify which index to use.

See:
- [AQL Index Hints](aql/operations-for.html#index-hints)
- [An introduction to index hints and named indices](https://www.arangodb.com/arangodb-training-center/index-hints-named-indices/){:target="_blank"}
  (tutorial)

### Sorted primary index (RocksDB engine)

The query optimizer can now make use of the sortedness of primary indexes if the
RocksDB engine is used. This means the primary index can be utilized for queries
that sort by either the `_key` or `_id` attributes of a collection and also for
range queries on these attributes.

In the list of documents for a collection in the web interface, the documents will 
now always be sorted in lexicographical order of their `_key` values. An exception for 
keys representing quasi-numerical values has been removed when doing the sorting in 
the web interface. Removing this exception can also speed up the display of the list
of documents.

This change potentially affects the order in which documents are displayed in the
list of documents overview in the web interface. A document with a key value "10" will 
now be displayed before a document with a key value of "9". In previous versions of
ArangoDB this was exactly opposite.

### Edge index query optimization (RocksDB engine)

An AQL query that uses the edge index only and returns the opposite side of
the edge can now be executed in a more optimized way, e.g.

    FOR edge IN edgeCollection FILTER edge._from == "v/1" RETURN edge._to

is fully covered by the RocksDB edge index. 

For MMFiles this rule does not apply.

### AQL syntax improvements

AQL now allows the usage of floating point values without leading zeros, e.g.
`.1234`. Previous versions of ArangoDB required a leading zero in front of
the decimal separator, i.e `0.1234`.

Also see: [AQL Numeric Literals](aql/fundamentals-data-types.html#numeric-literals)

### k Shortest Paths queries

AQL now allows to perform k Shortest Paths queries, that is, query a number of
paths of increasing length from a start vertex to a target vertex.

See: [AQL k Shortest Paths](aql/graphs-kshortest-paths.html)

SmartJoins
----------

The SmartJoins feature available in the ArangoDB Enterprise Edition allows
running joins between two sharded collections with performance close to that
of a local join operation.

The prerequisite for this is that the two collections have an identical sharding setup,
established via the `distributeShardsLike` attribute of one of the collections.

Quick example setup for two collections with identical sharding:

    > db._create("products", { numberOfShards: 3, shardKeys: ["_key"] });
    > db._create("orders", { distributeShardsLike: "products", shardKeys: ["productId"] });
    > db.orders.ensureIndex({ type: "hash", fields: ["productId"] });
    
Now an AQL query that joins the two collections via their shard keys will benefit from
the SmartJoin optimization, e.g.

    FOR p IN products 
      FOR o IN orders 
        FILTER p._key == o.productId 
        RETURN o

In this query's execution plan, the extra hop via the coordinator can be saved
that is normally there for generic joins. Thanks to the SmartJoin optimization,
the query's execution is as simple as:

    Execution plan:
     Id   NodeType                  Site  Est.   Comment
      1   SingletonNode             DBS      1   * ROOT
      3   EnumerateCollectionNode   DBS      9     - FOR o IN orders   /* full collection scan, 3 shard(s) */
      7   IndexNode                 DBS      0       - FOR p IN products   /* primary index scan, scan only, 3 shard(s) */
     10   RemoteNode                COOR     0         - REMOTE
     11   GatherNode                COOR     0         - GATHER
      6   ReturnNode                COOR     0         - RETURN o

Without the SmartJoin optimization, there will be an extra hop via the 
coordinator for shipping the data from each shard of the one collection to
each shard of the other collection, which will be a lot more expensive:

    Execution plan:
     Id   NodeType        Site  Est.   Comment
      1   SingletonNode   DBS      1   * ROOT
     16   IndexNode       DBS      3     - FOR p IN products   /* primary index scan, index only, projections: `_key`, 3 shard(s) */
     14   RemoteNode      COOR     3       - REMOTE
     15   GatherNode      COOR     3       - GATHER
      8   ScatterNode     COOR     3       - SCATTER
      9   RemoteNode      DBS      3       - REMOTE
      7   IndexNode       DBS      3       - FOR o IN orders   /* hash index scan, 3 shard(s) */
     10   RemoteNode      COOR     3         - REMOTE
     11   GatherNode      COOR     3         - GATHER
      6   ReturnNode      COOR     3         - RETURN o

In the end, SmartJoins can optimize away a lot of the inter-node network
requests normally required for performing a join between sharded collections.
The performance advantage of SmartJoins compared to regular joins will grow 
with the number of shards of the underlying collections.

In general, for two collections with `n` shards each, the minimal number of 
network requests for the general join (_no_ SmartJoins optimization) will be 
`n * (n + 2)`. The number of network requests increases quadratically with the 
number of shards. 

SmartJoins can get away with a minimal number of `n` requests here, which scales
linearly with the number of shards.

SmartJoins will also be especially advantageous for queries that have to ship a lot
of data around for performing the join, but that will filter out most of the data
after the join. In this case SmartJoins should greatly outperform the general join,
as they will eliminate most of the inter-node data shipping overhead.

Also see:
- [SmartJoins documentation](smartjoins.html)
- [SmartJoins tutorial](https://www.arangodb.com/arangodb-training-center/smart-joins/){:target="_blank"}

Background Index Creation
-------------------------

Creating new indexes is by default done under an exclusive collection lock. This means
that the collection (or the respective shards) are not available for write operations
as long as the index is created. This "foreground" index creation can be undesirable, 
if you have to perform it on a live system without a dedicated maintenance window.

Starting with ArangoDB 3.5, indexes can also be created in "background", not using an 
exclusive lock during the entire index creation. The collection remains basically available, 
so that other CRUD operations can run on the collection while the index is being created.
This can be achieved by setting the *inBackground* attribute when creating an index.

To create an index in the background in *arangosh* just specify `inBackground: true`, 
like in the following example:

```js
db.collection.ensureIndex({ type: "hash", fields: [ "value" ], inBackground: true });
```

Indexes that are still in the build process will not be visible via the ArangoDB APIs. 
Nevertheless it is not possible to create the same index twice via the *ensureIndex* API 
while an index is still being created. AQL queries also will not use these indexes until
the index reports back as fully created. Note that the initial *ensureIndex* call or HTTP 
request will still block until the index is completely ready. Existing single-threaded 
client programs can thus safely set the *inBackground* option to *true* and continue to 
work as before.

Should you be building an index in the background you cannot rename or drop the collection.
These operations will block until the index creation is finished. This is equally the case
with foreground indexing.

After an interrupted index build (i.e. due to a server crash) the partially built index
will the removed. In the ArangoDB cluster the index might then be automatically recreated 
on affected shards.

Background index creation might be slower than the "foreground" index creation and require 
more RAM. Under a write heavy load (specifically many remove, update or replace operations), 
the background index creation needs to keep a list of removed documents in RAM. This might 
become unsustainable if this list grows to tens of millions of entries.

Building an index is always a write-heavy operation, so it is always a good idea to build 
indexes during times with less load.

Please note that background index creation is useful only in combination with the RocksDB 
storage engine. With the MMFiles storage engine, creating an index will always block any
other operations on the collection.

Also see: [Creating Indexes in Background](indexing-index-basics.html#creating-indexes-in-background)

TTL (time-to-live) Indexes
--------------------------

The new TTL indexes feature provided by ArangoDB can be used for automatically 
removing expired documents from a collection. 

TTL indexes support eventual removal of documents which are past a configured
expiration timepoint. The expiration timepoints can be based upon the documents' 
original insertion or last-updated timepoints, with adding a period during
which to retain the documents.
Alternatively, expiration timepoints can be specified as absolute values per
document.
It is also possible to exclude documents from automatic expiration and removal.

Please also note that TTL indexes are designed exactly for the purpose of removing 
expired documents from collections. It is *not recommended* to rely on TTL indexes 
for user-land AQL queries. This is because TTL indexes internally may store a transformed, 
always numerical version of the index attribute value even if it was originally passed in 
as a datestring. As a result TTL indexes will likely not be used for filtering and sort 
operations in user-land AQL queries.

Also see: [TTL Indexes](indexing-ttl.html)

Collections
-----------

All collections now support a minimum replication factor (minReplicationFactor)
property. This is default set to `1`, which is identical to previous behavior.
If in a failover scenario a shard of a collection has less than minReplicationFactor
many in sync followers it will go into "read-only" mode and will reject writes
until enough followers are in sync again.

In more detail:
- Having `minReplicationFactor == 1` means as soon as a "master-copy" is
  available of the data writes are allowed.
- Having `minReplicationFactor > 1` requires additional in sync copies on
  follower servers to allow writes.

The feature is used to reduce the diverging of data in case of server failures
and to help new followers to catch up.

Also see: [Collection Database Methods](data-modeling-collections-database-methods.html#create)

HTTP API extensions
-------------------

### Extended index API

The HTTP API for creating indexes at POST `/_api/index` has been extended two-fold:

- to create a TTL (time-to-live) index, it is now possible to specify a value of `ttl`
  in the `type` attribute. When creating a TTL index, the attribute `expireAfter` is 
  also required. That attribute contains the expiration time (in seconds), which is
  based on the documents' index attribute value.

- to create an index in background, the attribute `inBackground` can be set to `true`.

### API for querying the responsible shard

The HTTP API for collections has got an additional route for retrieving the responsible
shard for a document at PUT `/_api/collection/<name>/responsibleShard`.

When calling this route, the request body is supposed to contain the document for which
the responsible shard should be determined. The response will contain an attribute `shardId`
containing the ID of the shard that is responsible for that document.

A method `collection.getResponsibleShard(document)` was added to the JS API as well.

It does not matter if the document actually exists or not, as the shard responsibility 
is determined from the document's attribute values only. 

Please note that this API is only meaningful and available on a cluster coordinator.

See:
- [Get responsible shard in JS API](data-modeling-collections-collection-methods.html#getresponsibleshard)
- [Get responsible shard in HTTP API](http/collection-getting.html#return-responsible-shard-for-a-document)

### Foxx API for running tests

The HTTP API for running Foxx service tests now supports a `filter` attribute,
which can be used to limit which test cases should be executed.

### Stream Transaction API

There is a new HTTP API for transactions. This API allows clients to add operations to a
transaction in a streaming fashion. A transaction can consist of a series of supported
transactional operations, followed by a commit or abort command.
This allows clients to construct transactions in a more natural way than
with JavaScript-based transactions.

Note that this requires client applications to abort transactions which are no 
longer necessary. Otherwise resources and locks acquired by the transactions
will be in use until the server decides to garbage-collect them.

In order to keep resource usage low, a maximum lifetime and transaction size for stream 
transactions is enforced on the coordinator to ensure that transactions cannot block the 
cluster from operating properly:

- Maximum idle timeout of **10 seconds** between operations
- Maximum transaction size of **128 MB** per DBServer

These limits are also enforced for stream transactions on single servers.

Enforcing the limits is useful to free up resources used by abandoned 
transactions, for example from transactions that are abandoned by client 
applications due to programming errors or that were left over because client 
connections were interrupted. Also see
[Known Issues](release-notes-known-issues35.html#stream-transactions)

See: [Stream Transaction HTTP API](http/transaction-stream-transaction.html)

### Minimal replication Factor

Within the properties of a collection we can now define a `minReplicationFactor`.
This affects all routes that can create or modify the properties of a collection,
including the graph API `_api/gharial`. All places where a replicationFactor can
be modified, can now modify the minReplicationFactor as well.

Web interface
-------------

When using the RocksDB engine, the selection of index types "hash" and "skiplist" 
has been removed from the web interface when creating new indexes. 

The index types "hash", "skiplist" and "persistent" are just aliases of each other 
when using the RocksDB engine, so there is no need to offer them all. In the web
interface there remains the index of type "persistent", which is feature-wise
identical with "hash" and "skiplist" indexes for the RocksDB engine.
Existing "hash" and "skiplist" indexes will remain fully functional.

JavaScript
----------

### V8 updated

The bundled version of the V8 JavaScript engine has been upgraded from 5.7.492.77 to 
7.1.302.28.

Among other things, the new version of V8 provides a native JavaScript `BigInt` type which 
can be used to store arbitrary-precision integers. However, to store such `BigInt` objects
in ArangoDB, they need to be explicitly converted to either strings or simple JavaScript
numbers.
Converting BigInts to strings for storage is preferred because converting a BigInt to a 
simple number may lead to precision loss.

```js
// will fail with "bad parameter" error:
value = BigInt("123456789012345678901234567890");
db.collection.insert({ value });

// will succeed:
db.collection.insert({ value: String(value) });

// will succeed, but lead to precision loss:
db.collection.insert({ value: Number(value) });
```

The new V8 version also changes the default timezone of date strings to be conditional 
on whether a time part is included:

```js
> new Date("2019-04-01");
Mon Apr 01 2019 02:00:00 GMT+0200 (Central European Summer Time)

> new Date("2019-04-01T00:00:00");
Mon Apr 01 2019 00:00:00 GMT+0200 (Central European Summer Time)
```

If the timezone is explicitly set in the date string, then the specified timezone will
always be honored: 

```js
> new Date("2019-04-01Z");
Mon Apr 01 2019 02:00:00 GMT+0200 (Central European Summer Time)
> new Date("2019-04-01T00:00:00Z");
Mon Apr 01 2019 02:00:00 GMT+0200 (Central European Summer Time)
```

### JavaScript Dependencies

More than a dozen JavaScript dependencies were updated in 3.5
([changelog](https://github.com/arangodb/arangodb/blob/3.5/CHANGELOG){:target="_blank"}).

The most significant one is the update of `joi` from 9.2.0 to 14.3.1. See the
respective [release notes](https://github.com/hapijs/joi/issues?q=is%3Aissue+label%3A%22release+notes%22){:target="_blank"}
to see if there are breaking changes for you.

Note that you can bundle your own version of `joi` if you need to rely on
version-dependent features.

### JavaScript Security Options

ArangoDB 3.5 provides several new options for restricting the functionality of
JavaScript application code running in the server, with the intent to make a setup
more secure.

There now exist startup options for restricting which environment variables and
values of which configuration options JavaScript code is allowed to read. These
options can be set to prevent leaking of confidential information from the
environment or the setup into the JavaScript application code.
Additionally there are options to restrict outbound HTTP connections from JavaScript
applications to certain endpoints and to restrict filesystem access from JavaScript
applications to certain directories only.

Finally there are startup options to turn off the REST APIs for managing Foxx
services, which can be used to prevent installation and uninstallation of Foxx
applications on a server. A separate option is provided to turn off access and
connections to the central Foxx app store via the web interface.

A complete overview of the security options can be found in
[Security Options](security-security-options.html).

### Foxx

Request credentials are now exposed via the `auth` property:

```js
const tokens = context.collection("tokens");
router.get("/authorized", (req, res) => {
  if (!req.auth || !req.auth.bearer || !tokens.exists(req.auth.bearer)) {
    res.throw(403, "Not authenticated");
  }
  // ...
});
```

### API improvements

Collections now provide the `documentId` method to derive document ids from keys.

Before:

```js
const collection = context.collection("users");
const documentKey = "my-document-key";
const documentId = `${collection.name()}/${documentKey}`;
```

After:

```js
const collection = context.collection("users");
const documentKey = "my-document-key";
const documentId = collection.documentId(documentKey);
```

Client tools
------------

### Dump and restore all databases

**arangodump** got an option `--all-databases` to make it dump all available databases
instead of just a single database specified via the option `--server.database`.

When set to true, this makes arangodump dump all available databases the current 
user has access to. The option `--all-databases` cannot be used in combination with 
the option `--server.database`. 

When `--all-databases` is used, arangodump will create a subdirectory with the data 
of each dumped database. Databases will be dumped one after the after. However, 
inside each database, the collections of the database can be dumped in parallel 
using multiple threads.
When dumping all databases, the consistency guarantees of arangodump are the same
as when dumping multiple single database individually, so the dump does not provide
cross-database consistency of the data.

**arangorestore** got an option `--all-databases` to make it restore all databases from
inside the subdirectories of the specified dump directory, instead of just the
single database specified via the option `--server.database`.

Using the option for arangorestore only makes sense for dumps created with arangodump 
and the `--all-databases` option. As for arangodump, arangorestore cannot be invoked 
with the both options `--all-databases` and `--server.database` at the same time. 
Additionally, the option `--force-same-database` cannot be used together with 
`--all-databases`.
  
If the to-be-restored databases do not exist on the target server, then restoring data 
into them will fail unless the option `--create-database` is also specified for
arangorestore. Please note that in this case a database user must be used that has 
access to the `_system` database, in order to create the databases on restore. 

Also see:
- [Arangodump](programs-arangodump.html)
- [Arangorestore](programs-arangorestore.html)

### Warning if connected to DBServer

Under normal circumstances there should be no need to connect to a 
database server in a cluster with one of the client tools, and it is 
likely that any user operations carried out there with one of the client
tools may cause trouble. 

The client tools arangosh, arangodump and arangorestore will now emit 
a warning when connecting with them to a database server node in a cluster.

Startup option changes
----------------------

The value type of the hidden startup option `--rocksdb.recycle-log-file-num` has 
been changed from numeric to boolean in ArangoDB 3.5, as the option is also a 
boolean option in the underlying RocksDB library.

Client configurations that use this configuration variable should adjust their
configuration and set this variable to a boolean value instead of to a numeric
value.

Miscellaneous
-------------

### Improved overview of available program options

The `--help-all` command-line option for all ArangoDB executables will now also 
show all hidden program options.

Previously hidden program options were only returned when invoking arangod or
a client tool with the cryptic `--help-.` option. Now `--help-all` simply returns 
them as well.

### Fewer system collections

The system collections `_frontend`, `_modules` and `_routing` are not created 
anymore for new databases by default. 

`_modules` and `_routing` are only needed for legacy functionality.
Existing `_routing` collections will not be touched as they may contain user-defined
entries, and will continue to work.

Existing `_modules` collections will also remain functional.

The `_frontend` collection may still be required for actions triggered by the
web interface, but it will automatically be created lazily if needed.

### Named indices

Indices now have an additional `name` field, which allows for more useful
identifiers. System indices, like the primary and edge indices, have default 
names (`primary` and `edge`, respectively). If no `name` value is specified
on index creation, one will be auto-generated (e.g. `idx_13820395`). The index
name _cannot_ be changed after index creation. No two indices on the same
collection may share the same name, but two indices on different collections 
may.

### ID values in log messages

By default, ArangoDB and its client tools now show a 5 digit unique ID value in
any of their log messages, e.g.

    2019-03-25T21:23:19Z [8144] INFO [cf3f4] ArangoDB (version 3.5.0 enterprise [linux]) is ready for business. Have fun!.

In this message, the `cf3f4` is the message's unique ID value. ArangoDB users can
use this ID to build custom monitoring or alerting based on specific log ID values.
Existing log ID values are supposed to stay constant in future releases of arangod.

Additionally the unique log ID values can be used by the ArangoDB support to find
out which component of the product exactly generated a log message. The IDs also
make disambiguation of identical log messages easier.

The presence of these ID values in log messages may confuse custom log message filtering 
or routing mechanisms that parse log messages and that rely on the old log message
format.

This can be fixed adjusting any existing log message parsers and making them aware
of the ID values. The ID values are always 5 byte strings, consisting of the characters
`[0-9a-f]`. ID values are placed directly behind the log level (e.g. `INFO`).

Alternatively, the log IDs can be suppressed in all log messages by setting the startup
option `--log.ids false` when starting arangod or any of the client tools.

Internal
--------

We have moved from C++11 to C++14, which allows us to use some of the simplifications,
features and guarantees that this standard has in stock.
To compile ArangoDB from source, a compiler that supports C++14 is now required.

The bundled JEMalloc memory allocator used in ArangoDB release packages has been
upgraded from version 5.0.1 to version 5.2.0.

The bundled version of the RocksDB library has been upgraded from 5.16 to 6.2.

The unit test framework has been changed from catch to googletest. This change also
renames a CMake configuration variable from `USE_CATCH_TESTS` to `USE_GOOGLE_TESTS`.
