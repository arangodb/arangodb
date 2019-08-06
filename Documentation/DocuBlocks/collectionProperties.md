
@startDocuBlock collectionProperties
@brief gets or sets the properties of a collection
`collection.properties()`

Returns an object containing all collection properties.

* *waitForSync*: If *true* creating a document will only return
  after the data was synced to disk.

* *journalSize* : The size of the journal in bytes.
  This option is meaningful for the MMFiles storage engine only.

* *isVolatile*: If *true* then the collection data will be
  kept in memory only and ArangoDB will not write or sync the data
  to disk.
  This option is meaningful for the MMFiles storage engine only.

* *keyOptions* (optional) additional options for key generation. This is
  a JSON array containing the following attributes (note: some of the
  attributes are optional):
  * *type*: the type of the key generator used for the collection.
  * *allowUserKeys*: if set to *true*, then it is allowed to supply
    own key values in the *_key* attribute of a document. If set to
    *false*, then the key generator will solely be responsible for
    generating keys and supplying own key values in the *_key* attribute
    of documents is considered an error.
  * *increment*: increment value for *autoincrement* key generator.
    Not used for other key generator types.
  * *offset*: initial offset value for *autoincrement* key generator.
    Not used for other key generator types.

* *indexBuckets*: number of buckets into which indexes using a hash
  table are split. The default is 16 and this number has to be a
  power of 2 and less than or equal to 1024.
  This option is meaningful for the MMFiles storage engine only.

  For very large collections one should increase this to avoid long pauses
  when the hash table has to be initially built or resized, since buckets
  are resized individually and can be initially built in parallel. For
  example, 64 might be a sensible value for a collection with 100
  000 000 documents. Currently, only the edge index respects this
  value, but other index types might follow in future ArangoDB versions.
  Changes (see below) are applied when the collection is loaded the next
  time.

In a cluster setup, the result will also contain the following attributes:

* *numberOfShards*: the number of shards of the collection.

* *shardKeys*: contains the names of document attributes that are used to
  determine the target shard for documents.

* *replicationFactor*: determines how many copies of each shard are kept 
  on different DBServers. Has to be in the range of 1-10 *(Cluster only)*

* *minReplicationFactor* : determines the number of minimal shard copies kept on
  different DBServers, a shard will refuse to write if less than this amount
  of copies are in sync. Has to be in the range of 1-replicationFactor *(Cluster only)*

* *shardingStrategy*: the sharding strategy selected for the collection.
  This attribute will only be populated in cluster mode and is not populated
  in single-server mode.

`collection.properties(properties)`

Changes the collection properties. *properties* must be an object with
one or more of the following attribute(s):

* *waitForSync*: If *true* creating a document will only return
  after the data was synced to disk.

* *journalSize* : The size of the journal in bytes.
  This option is meaningful for the MMFiles storage engine only.

* *indexBuckets* : See above, changes are only applied when the
  collection is loaded the next time.
  This option is meaningful for the MMFiles storage engine only.

* *replicationFactor* : Change the number of shard copies kept on 
  different DBServers, valid values are  integer numbers
  in the range of 1-10 *(Cluster only)*

* *minReplicationFactor* : Change the number of minimal shard copies to be in sync on 
  different DBServers, a shard will refuse to write if less than this amount
  of copies are in sync. Has to be in the range of 1-replicationFactor *(Cluster only)*

**Note**: some other collection properties, such as *type*, *isVolatile*,
*keyOptions*, *numberOfShards* or *shardingStrategy* cannot be changed once 
the collection is created.

@EXAMPLES

Read all properties

@EXAMPLE_ARANGOSH_OUTPUT{collectionProperties}
~ db._create("example");
  db.example.properties();
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

Change a property

@EXAMPLE_ARANGOSH_OUTPUT{collectionProperty}
~ db._create("example");
  db.example.properties({ waitForSync : true });
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

@endDocuBlock

