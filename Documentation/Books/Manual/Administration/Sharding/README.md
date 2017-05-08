Sharding
========

ArangoDB is organizing its collection data in shards. Sharding
allows to use multiple machines to run a cluster of ArangoDB
instances that together constitute a single database. This enables
you to store much more data, since ArangoDB distributes the data 
automatically to the different servers. In many situations one can 
also reap a benefit in data throughput, again because the load can
be distributed to multiple machines.

Shards are configured per collection so multiple shards of data form
the collection as a whole. To determine in which shard the data is to
be stored ArangoDB performs a hash across the values. By default this
hash is being created from _key.

To configure the number of shards:

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4});
```

To configure the hashing for another attribute:

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4, "shardKeys": ["country"]});
```

This would be useful to keep data of every country in one shard which
would result in better performance for queries working on a per country
base. You can also specify multiple `shardKeys`. Note however that if
you change the shard keys from their default `["_key"]`, then finding
a document in the collection by its primary key involves a request to
every single shard. Furthermore, in this case one can no longer prescribe
the primary key value of a new document but must use the automatically
generated one. This latter restriction comes from the fact that ensuring
uniqueness of the primary key would be very inefficient if the user
could specify the primary key.

On which node in a cluster a particular shard is kept is undefined.
There is no option to configure an affinity based on certain shard keys.

Unique indexes (hash, skiplist, persistent) on sharded collections are
only allowed if the fields used to determine the shard key are also
included in the list of attribute paths for the index:

| shardKeys | indexKeys |        |
|----------:|----------:|-------:|
| a         | a         |     ok |
| a         | b         | not ok |
| a         | a, b      |     ok |
| a, b      | a         | not ok |
| a, b      | b         | not ok |
| a, b      | a, b      |     ok |
| a, b      | a, b, c   |     ok |
| a, b, c   | a, b      | not ok |
| a, b, c   | a, b, c   |     ok |
