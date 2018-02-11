Sharding
========

For an introduction about Sharding in Cluster, please refer to the
[_Cluster Architecture_](../../Scalability/Architecture.md) section. 

Number of _shards_ can be configured at _collection_ creation time, e.g. the UI,
or the _ArangoDB Shell_:

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4});
```

To configure a custom _hashing_ for another attribute (default is __key_):

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4, "shardKeys": ["country"]});
```

The example above, where 'country' has been used as _shardKeys_ can be useful
to keep data of every country in one shard, which would result in better
performance for queries working on a per country base. 

It is also possible to specify multiple `shardKeys`. 

Note however that if you change the shard keys from their default `["_key"]`, then finding
a document in the collection by its primary key involves a request to
every single shard. Furthermore, in this case one can no longer prescribe
the primary key value of a new document but must use the automatically
generated one. This latter restriction comes from the fact that ensuring
uniqueness of the primary key would be very inefficient if the user
could specify the primary key.

On which DBServer in a Cluster a particular _shard_ is kept is undefined.
There is no option to configure an affinity based on certain _shard_ keys.

Unique indexes (hash, skiplist, persistent) on sharded collections are
only allowed if the fields used to determine the shard key are also
included in the list of attribute paths for the index:

| shardKeys | indexKeys |             |
|----------:|----------:|------------:|
| a         | a         |     allowed |
| a         | b         | not allowed |
| a         | a, b      |     allowed |
| a, b      | a         | not allowed |
| a, b      | b         | not allowed |
| a, b      | a, b      |     allowed |
| a, b      | a, b, c   |     allowed |
| a, b, c   | a, b      | not allowed |
| a, b, c   | a, b, c   |     allowed |
