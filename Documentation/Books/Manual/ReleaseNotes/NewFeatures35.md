Features and Improvements in ArangoDB 3.5
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.5. ArangoDB 3.5 also contains several bug fixes that are not listed
here.

AQL
---

### SORT-LIMIT optimization

A new SORT-LIMIT optimization has been added. This optimization will be pulled off
by the query optimizer if there is a SORT statement followed by a LIMIT node, and the
overall number of documents to return is relatively small in relation to the total
number of documents to be sorted. In this case, the optimizer will use a size-constrained
heap for keeping only the required number of results in memory, which can drastically
reduce memory usage and, for some queries, also execution time for the sorting.

If the optimization is applied, it will show as "sort-limit" rule in the query execution
plan.

### Index Hints in AQL

Users may now take advantage of the `indexHint` inline query option to override
the internal optimizer decision regarding which index to use to serve content
from a given collection. The index hint works with the named indices feature
above, making it easy to specify which index to use.

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


Smart Joins
-----------

The "smart joins" feature available in the ArangoDB Enterprise Edition allows running 
joins between two sharded collections with performance close to that of a local join 
operation. 

The prerequisite for this is that the two collections have an identical sharding setup,
established via the `distributeShardsLike` attribute of one of the collections.

Quick example setup for two collections with identical sharding:

    > db._create("products", { numberOfShards: 3, shardKeys: ["_key"] });
    > db._create("orders", { distributeShardsLike: "products", shardKeys: ["productId"] });
    > db.orders.ensureIndex({ type: "hash", fields: ["productId"] });
    
Now an AQL query that joins the two collections via their shard keys will benefit from
the smart join optimization, e.g.

    FOR p IN products 
      FOR o IN orders 
        FILTER p._key == o.productId 
        RETURN o

In this query's execution plan, the extra hop via the coordinator can be saved
that is normally there for generic joins. Thanks to the smart join optimization,
the query's execution is as simple as:

    Execution plan:
     Id   NodeType                  Site  Est.   Comment
      1   SingletonNode             DBS      1   * ROOT
      3   EnumerateCollectionNode   DBS      9     - FOR o IN orders   /* full collection scan, 3 shard(s) */
      7   IndexNode                 DBS      0       - FOR p IN products   /* primary index scan, scan only, 3 shard(s) */
     10   RemoteNode                COOR     0         - REMOTE
     11   GatherNode                COOR     0         - GATHER
      6   ReturnNode                COOR     0         - RETURN o

Without the smart join optimization, there will be an extra hop via the 
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

In the end, smart joins can optimize away a lot of the inter-node network
requests normally required for performing a join between sharded collections.
The performance advantage of smart joins compared to regular joins will grow 
with the number of shards of the underlying collections.

In general, for two collections with `n` shards each, the minimal number of 
network requests for the general join (_no_ smart joins optimization) will be 
`n * (n + 2)`. The number of network requests increases quadratically with the 
number of shards. 

Smart joins can get away with a minimal number of `n` requests here, which scales
linearly with the number of shards.

Smart joins will also be especially advantageous for queries that have to ship a lot
of data around for performing the join, but that will filter out most of the data
after the join. In this case smart joins should greatly outperform the general join,
as they will eliminate most of the inter-node data shipping overhead.

Also see the [Smart Joins](../SmartJoins.md) page.


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
while an index is still begin created. AQL queries also will not use these indexes until
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


TTL (time-to-live) Indexes
--------------------------

The new TTL indexes provided by ArangoDB can be used for removing expired documents
from a collection. 

A TTL index can be set up by setting an `expireAfter` value and by picking a single 
document attribute which contains the documents' creation date and time. Documents 
are expired after `expireAfter` seconds after their creation time. The creation time
is specified as either a numeric timestamp or a UTC datestring.

For example, if `expireAfter` is set to 600 seconds (10 minutes) and the index
attribute is "creationDate" and there is the following document:

    { "creationDate" : 1550165973 }

This document will be indexed with a creation timestamp value of `1550165973`,
which translates to the human-readable date string `2019-02-14T17:39:33.000Z`. The 
document will expire 600 seconds afterwards, which is at timestamp `1550166573000` (or
`2019-02-14T17:49:33.000Z` in the human-readable version).

The actual removal of expired documents will not necessarily happen immediately. 
Expired documents will eventually removed by a background thread that is periodically
going through all TTL indexes and removing the expired documents.

There is no guarantee when exactly the removal of expired documents will be carried
out, so queries may still find and return documents that have already expired. These
will eventually be removed when the background thread kicks in and has capacity to
remove the expired documents. It is guaranteed however that only documents which are 
past their expiration time will actually be removed.

Please note that the numeric timestamp values for the index attribute should be 
specified in seconds since January 1st 1970 (Unix timestamp). To calculate the current 
timestamp from JavaScript in this format, there is `Date.now() / 1000`, to calculate it 
from an arbitrary Date instance, there is `Date.getTime() / 1000`.

Alternatively, the index attribute values can be specified as a date string in format
`YYYY-MM-DDTHH:MM:SS` with optional milliseconds. All date strings will be interpreted 
as UTC dates.
    
The above example document using a datestring attribute value would be

    { "creationDate" : "2019-02-14T17:39:33.000Z" }

In case the index attribute does not contain a numeric value nor a proper date string, 
the document will not be stored in the TTL index and thus will not become a candidate 
for expiration and removal. Providing either a non-numeric value or even no value for 
the index attribute is a supported way of keeping documents from being expired and removed.

There can at most be one TTL index per collection. It is not recommended to use
TTL indexes for user-land AQL queries, as TTL indexes may store a transformed,
always numerical version of the index attribute value.

The frequency for invoking the background removal thread can be configured 
using the `--ttl.frequency` startup option. 
In order to avoid "random" load spikes by the background thread suddenly kicking 
in and removing a lot of documents at once, the number of to-be-removed documents
per thread invocation can be capped.
The total maximum number of documents to be removed per thread invocation is
controlled by the startup option `--ttl.max-total-removes`. The maximum number of
documents in a single collection at once can be controlled by the startup option
`--ttl.max-collection-removes`.


HTTP API extensions
-------------------

The HTTP API for creating indexes at POST `/_api/index` has been extended two-fold:

* to create a TTL (time-to-live) index, it is now possible to specify a value of `ttl`
  in the `type` attribute. When creating a TTL index, the attribute `expireAfter` is 
  also required. That attribute contains the expiration time (in seconds), which is
  based on the documents' index attribute value.

* to create an index in background, the attribute `inBackground` can be set to `true`.


Web interface
-------------

When using the RocksDB engine, the selection of index types "persistent" and "skiplist" 
has been removed from the web interface when creating new indexes. 

The index types "hash", "skiplist" and "persistent" are just aliases of each other 
when using the RocksDB engine, so there is no need to offer them all.


JavaScript
----------

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
a client tool with the cryptic `--help-.` option. Now `--help-all` simply retuns 
them as well.

### Fewer system collections

The system collections `_routing` and `_modules` are not created anymore for new
new databases, as both are only needed for legacy functionality.

Existing `_routing` collections will not be touched as they may contain user-defined
entries, and will continue to work.

Existing `_modules` collections will also remain functional.

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
upgraded from version 5.0.1 to version 5.1.0.

The bundled version of the RocksDB library has been upgraded from 5.16 to 6.0.
