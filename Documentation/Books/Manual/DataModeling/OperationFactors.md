Data Modeling and Operational Factors
====================================

Design the data model of your application is a crucial task that can make or break the performance
of your application. A well-designed data model will allow you to write efficient AQL queries, increase
throughput of CRUD operations and will make sure your data is distributed in the most effective way.

Whether you design a new application with ArangoDB or port an existing one to use ArangoDB, you should
always analyze the (expected) data access patterns of your application in conjunction with several factors:

Operation Atomicity
-------------------

All insert / update / replace / remove operations in ArangoDB are atomic on a _single_ document.
Using a single instance of ArangoDB, multi-document / multi-collection queries are guaranteed to be fully ACID, however
in cluster mode only single-document operations are also fully ACID. This has implications if you try to ensure
consistency across multiple operations.

### Denormalizing Data

In traditional _SQL_ databases it is considered a good practice to normalize all your data across multiple tables
to avoid duplicated data and ensure consistency.

ArangoDB is a schema-less _NoSQL_ document database so a good data model is not necessarily normalized. On the contrary to
avoid extra joins it is often an advantage to deliberately _denormalize_ your datamodel.

To denormalize your data model you essentially combine all related entities into a single document instead of spreading it
over multiple documents and collections. The advantage of this is that it allows you to atomically update all of your
connected data, the downside is that your documents become larger (see below for more considerations on large documents).

As a simple example, lets say you want to maintain the balance of a shopping basket (from an online shop) together with a list of all included items and prices.
The total balance of all items in the shopping basket should stay in sync with the contained items, then you may put
all contained items inside the shopping basket document and only update them together:

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

### Ensuring Consistent Atomic Updates

There are ways to ensure atomicity and consistency when perfoming updates in your application.
ArangoDB allows you to specify the revision ID of the existing document you would like to update.
This way you can ensure that if your application has read a document with a certain revision, the
modifications to it are only allowed to pass if and only if the document was not changed by someone else.

You can specify the revision via the `_rev` field inside the document or via the `If-Match: <revision>`
header in the documents REST API. In the _arangosh_ you can peform such an operation like this:

```js
db.basketCollection.update({"_key": "123", "_rev": "_Xv0TA0O--_"}, data)
// or replace
db.basketCollection.replace({"_key": "123", "_rev": "_Xv0TA0O--_"}, data)
```

Indexes
-------

Indexes can improve the performance of AQL queries drastically. Queries that frequently filter on one more fields
can be made faster by creating an index via the _ensureIndex_ command. There is already a permanent primary
index in every collection on the *_key* and *_id* fields as well as the edge index on *_from* and *_to* (for edge collections).

Should you decide to create an index you should consider a few things:

- Indexes are a trade-off between storage space and query speed.
- Each new index will increase the amount of RAM and (for the rocksdb storage) the amount of disk space needed.
- Indexes with [indexed array values](../Indexing/IndexBasics.md#indexing-array-values) need an extra index entry per array entry
- Consider the extra space requirements of indexes when planning server capacities.
- Adding indexes increases the write-amplification i.e. it negatively affects the write performance
- Each index needs to add at least one index entry per document. You can use _sparse indexes_ to avoid adding index entries for unused fields
- Collections that are more frequently read benefit the most from added indexes, provided you actually use the indexes
- Collections with a high rate of inserts or updates compared to reads may hurt overall performance.

Generally it is best to design your indexes with your queries in mind. Use the [query profiler](../../AQL/ExecutionAndPerformance/QueryProfiler.html) to understand the bottlenecks in your queries. For more information on indexes see [Index Basics](../Indexing/IndexBasics.md)

<!-- TODO maybe expand a bit on capacity planning ? -->

Number of Databases and Collections
-----------------------------------

Sometimes you can consider to split up data over multiple collections. For example one could create a
new set of collections for each new customer, instead of having a customer field on each documents. Having a
large number of collections has no performance penalty and results in good performance.

Grouping documents into collections by type (i.e. a session collection 'sessions_dev', 'sessions_prod') allows
you to avoid an index on the type field.

A few things to consider:
- Adding an extra collection always incurs a small amount of overhead for the collection metadata and indexes.
- Only with the _MMFiles storage engine_, creating extra databases will require two compaction and cleanup threads per database.
  This might lead to undesirable effects should you decide to create many databases compared to the number of available CPU cores.

Cluster Sharding
----------------

The ArangoDB cluster _partitions_ your collections into one or more _shards_ across multiple _DBServers_.
This enables efficient _horizontal scaling_: It allows you to store much more data, since ArangoDB distributes the data
automatically to the different servers. In many situations one can also reap a benefit in data throughput,
again because the load can be distributed to multiple machines.

ArangoDB uses the specified _shard keys_ to determine in which shard a given document is stored. Choosing the right
shard key can have significant inpact on your performance can reduce network traffice and increase performance,

ArangoDB uses consistent hashing to compute the target shard from the given values (as specified via 'shardKeys').
The ideal set of shard keys allows ArangoDB to distribute documents evenly across your shards and your _DBServers_.
By default ArangoDB uses the `_key` field as a shard key. For a custom shard key
you should consider a few different properties:

- **Cardinality**: The cardinality of a set is the number of distinct values that it contains. A shard key with
  only _N_ distinct values can not be hashed onto more than _N_ shards. Consider using multiple shard keys, if one of your values has a low cardinality.
- **Frequency**: Consider how often a given shard key value may appear in your data. Having a lot of documents with identical shard   keys will lead to unevenly distributed data.

See [Sharding](../Architecture/DeploymentModes/Cluster/Architecture.md#Sharding) for more information

Document and Transaction Size
-----------------------------

When designing your data-model you should keep in mind that the size of documents affects the performance
in the single-server and cluster setup.
Each document has a certain amount of overhead, depending on the storage engine and the indexes you added
to the collection. The overhead may become significant if your store a large amount of very small documents.

Consider some ways to minimize the required amount of storage space:

- Explicitly set the `_key` field to a custom unique value. This enables you to store information in the `_key`field
  instead of another field inside the document. The `_key` value is always indexed, setting a custom value means you can use a shorter
value than what would have been generated automatically.
- Shorter field names will reduce the amount of space needed to store documents (This has not effect on index size).
  ArangoDB is schemaless and needs to store the document structure inside each document; Usually this is a small overhead compared to the overall document size.

Especially for the _rocksdb_ storage engine large documents and transactions may negatively inpact the write performance:
- Consider a max size of 50-75 kB _per document_ a good rule of thumb. This will allow you to maintain steady write
  throughput under high load.
- Transactions are held in-memory before they are committed. This means that transactions have to be split if they become too big,    see the [limitations section](../Transactions/Limitations.md#with-rocksdb-storage-engine)
