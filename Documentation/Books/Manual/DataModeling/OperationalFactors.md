Data Modeling and Operational Factors
=====================================

Designing the data model of your application is a crucial task that can make or
break the performance of your application. A well-designed data model will
allow you to write efficient AQL queries, increase throughput of CRUD operations
and will make sure your data is distributed in the most effective way.

Whether you design a new application with ArangoDB or port an existing one to
use ArangoDB, you should always analyze the (expected) data access patterns of
your application in conjunction with several factors:

Operation Atomicity
-------------------

All insert / update / replace / remove operations in ArangoDB are atomic on a
_single_ document. Using a single instance of ArangoDB, multi-document /
multi-collection queries are guaranteed to be fully ACID, however in
cluster mode only single-document operations are also fully ACID. This has
implications if you try to ensure consistency across multiple operations.

### Denormalizing Data

In traditional _SQL_ databases it is considered a good practice to normalize
all your data across multiple tables to avoid duplicated data and ensure
consistency.

ArangoDB is a schema-less _NoSQL_ multi-model database, so a good data model
is not necessarily normalized. On the contrary, to avoid extra joins it is
often an advantage to deliberately _denormalize_ your data model.

To denormalize your data model you essentially combine all related entities
into a single document instead of spreading it over multiple documents and
collections. The advantage of this is that it allows you to atomically update
all of your connected data, the downside is that your documents become larger
(see below for more considerations on
[large documents](#document-and-transaction-sizes)).

As a simple example, lets say you want to maintain the total amount of a
shopping basket (from an online shop) together with a list of all included
items and prices. The total balance of all items in the shopping basket should
stay in sync with the contained items, then you may put all contained items
inside the shopping basket document and only update them together:

```json
{
    "_id": "basket/123",
    "_key": "123",
    "_rev": "_Xv0TA0O--_",
    "user": "some_user",
    "balance": "100",
    "items": [ { "price": 10, "title": "Harry Potter and the Philosopher’s Stone" },
               { "price": 90, "title": "Vacuum XYZ" } ]
}
```

This allows you to avoid making lookups via the document keys in
multiple collections.

### Ensuring Consistent Atomic Updates

There are ways to ensure atomicity and consistency when performing updates in
your application. ArangoDB allows you to specify the revision ID (`_rev`) value
of the existing document you want to update. The update or replace operation is
only able to succeed if the values match. This way you can ensure that if your
application has read a document with a certain `_rev` value, the modifications
to it are only allowed to pass _if and only if_ the document was not changed by
someone else in the meantime. By specifying a document's previous revision ID
you can avoid losing updates on these documents without noticing it.

You can specify the revision via the `_rev` field inside the document or via
the `If-Match: <revision>` HTTP header in the documents REST API.
In the _arangosh_ you can perform such an operation like this:

```js
db.basketCollection.update({"_key": "123", "_rev": "_Xv0TA0O--_"}, data)
// or replace
db.basketCollection.replace({"_key": "123", "_rev": "_Xv0TA0O--_"}, data)
```

An AQL query with the same effect can be written by using the _ignoreRevs_
option together with a modification operation. Either let ArangoDB compare
the `_rev` value and  only succeed if they still match, or let ArangoDB
ignore them (default):

```js
FOR i IN 1..1000
  UPDATE { _key: CONCAT('test', i), _rev: "1287623" }
  WITH { foobar: true } IN users
  OPTIONS { ignoreRevs: false }
```

Indexes
-------

Indexes can improve the performance of AQL queries drastically. Queries that
frequently filter on or one more fields can be made faster by creating an index
(in arangosh via the _ensureIndex_ command, the Web UI or your specific
client driver). There is already an automatic (and non-deletable) primary index
in every collection on the `_key` and `_id` fields as well as the edge index
on `_from` and `_to` (for edge collections).

Should you decide to create an index you should consider a few things:

- Indexes are a trade-off between storage space, maintenance cost and query speed.
- Each new index will increase the amount of RAM and (for the RocksDB storage)
  the amount of disk space needed.
- Indexes with [indexed array values](../Indexing/IndexBasics.md#indexing-array-values)
  need an extra index entry per array entry
- Adding indexes increases the write-amplification i.e. it negatively affects
  the write performance (how much depends on the storage engine)
- Each index needs to add at least one index entry per document. You can use
  _sparse indexes_ to avoid adding _null_ index entries for rarely used attributes
- Sparse indexes can be smaller than non-sparse indexes, but they can only be
  used if the optimizer determines that the _null_ value cannot be in the
  result range, e.g. by an explicit `FILTER doc.attribute != null` in AQL
  (also see [Type and value order](../../AQL/Fundamentals/TypeValueOrder.html)).
- Collections that are more frequently read benefit the most from added indexes,
  provided the indexes can actually be utilized
- Indexes on collections with a high rate of inserts or updates compared to
  reads may hurt overall performance.

Generally it is best to design your indexes with your queries in mind.
Use the [query profiler](../../AQL/ExecutionAndPerformance/QueryProfiler.html)
to understand the bottlenecks in your queries.

Always consider the additional space requirements of extra indexes when
planning server capacities. For more information on indexes see
[Index Basics](../Indexing/IndexBasics.md).

<!-- TODO eventually add a page on capacity planning -->

Number of Databases and Collections
-----------------------------------

Sometimes you can consider to split up data over multiple collections.
For example, one could create a new set of collections for each new customer
instead of having a customer field on each documents. Having a few thousand
collections has no significant performance penalty for most operations and
results in good performance.

Grouping documents into collections by type (i.e. a session collection
'sessions_dev', 'sessions_prod') allows you to avoid an extra index on a _type_
field. Similarly you may consider to
[split edge collections](../Graphs/README.md#multiple-edge-collections-vs-filters-on-edge-document-attributes)
instead of specifying the type of the connection inside the edge document.

A few things to consider:
- Adding an extra collection always incurs a small amount of overhead for the
  collection metadata and indexes.
- You cannot use more than _2048_ collections per AQL query
- Uniqueness constraints on certain attributes (via an unique index) can only
  be enforced by ArangoDB within one collection
- Only with the _MMFiles storage engine_: Creating extra databases will require
  two compaction and cleanup threads per database. This might lead to
  undesirable effects should you decide to create many databases compared to
  the number of available CPU cores.

Cluster Sharding
----------------

The ArangoDB cluster _partitions_ your collections into one or more _shards_
across multiple _DBServers_. This enables efficient _horizontal scaling_:
It allows you to store much more data, since ArangoDB distributes the data
automatically to the different servers. In many situations one can also reap
a benefit in data throughput, again because the load can be distributed to
multiple machines.

ArangoDB uses the specified _shard keys_ to determine in which shard a given
document is stored. Choosing the right shard key can have significant impact on
your performance can reduce network traffic and increase performance.

ArangoDB uses consistent hashing to compute the target shard from the given
values (as specified via 'shardKeys'). The ideal set of shard keys allows
ArangoDB to distribute documents evenly across your shards and your _DBServers_.
By default ArangoDB uses the `_key` field as a shard key. For a custom shard key
you should consider a few different properties:

- **Cardinality**: The cardinality of a set is the number of distinct values
  that it contains. A shard key with only _N_ distinct values can not be hashed
  onto more than _N_ shards. Consider using multiple shard keys, if one of your
  values has a low cardinality.
- **Frequency**: Consider how often a given shard key value may appear in
  your data. Having a lot of documents with identical shard keys will lead
  to unevenly distributed data.

See [Sharding](../Architecture/DeploymentModes/Cluster/Architecture.md#sharding)
for more information

### Smart Graphs

Smart Graphs are an Enterprise Edition feature of ArangoDB. It enables you to
manage graphs at scale, it will give a vast performance benefit for all graphs
sharded in an ArangoDB Cluster.

To add a Smart Graph you need a smart graph attribute that partitions your
graph into several smaller sub-graphs. Ideally these sub-graphs follow a
"natural" structure in your data. These subgraphs have a large amount of edges
that only connect vertices in the same subgraph and only have few edges
connecting vertices from other subgraphs.

All the usual considerations for sharding keys also apply for smart attributes,
for more information see [SmartGraphs](../Graphs/SmartGraphs/README.md)

Document and Transaction Sizes
------------------------------

When designing your data-model you should keep in mind that the size of
documents affects the performance and storage requirements of your system.
Very large numbers of very small documents may have an unexpectedly big overhead:
Each document needs has a certain amount extra storage space, depending on the
storage engine and the indexes you added to the collection. The overhead may
become significant if your store a large amount of very small documents.

Very large documents may reduce your write throughput:
This is due to the extra time needed to send larger documents over the
network as well as more copying work required inside the storage engines.

Consider some ways to minimize the required amount of storage space:

- Explicitly set the `_key` field to a custom unique value.
  This enables you to store information in the `_key` field instead of another
  field inside the document. The `_key` value is always indexed, setting a
  custom value means you can use a shorter value than what would have been
  generated automatically.
- Shorter field names will reduce the amount of space needed to store documents
  (this has no effect on index size). ArangoDB is schemaless and needs to store
  the document structure inside each document. Usually this is a small overhead
  compared to the overall document size.
- Combining many small related documents into one larger one can also
  reduce overhead. Common fields can be stored once and indexes just need to
  store one entry. This will only be beneficial if the combined documents are
  regularly retrieved together and not just subsets.

RockDB Storage Engine
---------------------

Especially for the RocksDB storage engine large documents and transactions may
negatively impact the write performance:
- Consider a maximum size of 50-75 kB _per document_ as a good rule of thumb.
  This will allow you to maintain steady write throughput even under very high load.
- Transactions are held in-memory before they are committed.
  This means that transactions have to be split if they become too big, see the
  [limitations section](../Transactions/Limitations.md#with-rocksdb-storage-engine).

### Improving Update Query Perfromance

You may use the _exclusive_ query option for modifying AQL queries, to improve the performance drastically.
This has the downside that no concurrent writes may occur on the collection, but ArangoDB is able
to use a special fast-path which should improve the performance by up to 50% for large collections.

```js
FOR doc IN mycollection
  UPDATE doc._key
  WITH { foobar: true } IN mycollection
  OPTIONS { exclusive: true }
```

The same naturally also applies for queries using _REPLACE_ or _INSERT_. Additionally you may be able to use
the `intermediateCommitCount` option in the API to subdivide the AQL transaction into smaller batches.

### Read / Write Load Balance

Depending on whether your data model has a higher read- or higher write-rate you may want
to adjust some of the RocksDB specific options. Some of the most critical options to
adjust the performance and memory usage are listed below:

`--rocksdb.block-cache-size`

This is the size of the block cache in bytes. This cache is used for read operations.
Increasing the size of this may improve the performance of read heavy workloads.
You may wish to adjust this parameter to control memory usage.

`--rocksdb.write-buffer-size`

Amount of data to build up in memory before converting to a file on disk.
Larger values increase performance, especially during bulk loads.

`--rocksdb.max-write-buffer-number`

Maximum number of write buffers that built up in memory, per internal column family.
The default and the minimum number is 2, so that when 1 write buffer
is being flushed to storage, new writes can continue to the other write buffer.

`--rocksdb.total-write-buffer-size`

The total amount of data to build up in all in-memory buffers when writing into ArangoDB.
You may wish to adjust this parameter to control memory usage.

Setting this to a low value may limit the RAM that ArangoDB will use but may slow down
write heavy workloads. Setting this to 0 will not limit the size of the write-buffers.

`--rocksdb.level0-stop-trigger`

When this many files accumulate in level-0, writes will be stopped to allow compaction to catch up.
Setting this value very high may improve write throughput, but may lead to temporarily 
bad read performance.
