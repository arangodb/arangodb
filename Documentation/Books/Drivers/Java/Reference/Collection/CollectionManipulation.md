<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating the collection

These functions implement
[the HTTP API for modifying collections](../../../..//HTTP/Collection/Modifying.html).

## ArangoDatabase.createCollection

`ArangoDatabase.createCollection(String name, CollectionCreateOptions options) : CollectionEntity`

Creates a collection with the given _options_ for this collection's name,
then returns collection information from the server.

**Arguments**

- **name**: `String`

  The name of the collection

- **options**: `CollectionCreateOptions`

  - **journalSize**: `Long`

    The maximal size of a journal or datafile in bytes.
    The value must be at least 1048576 (1 MiB).

  - **replicationFactor**: `Integer`

    (The default is 1): in a cluster, this attribute determines how many copies
    of each shard are kept on different DBServers. The value 1 means that only
    one copy (no synchronous replication) is kept. A value of k means that
    k-1 replicas are kept. Any two copies reside on different DBServers.
    Replication between them is synchronous, that is, every write operation to
    the "leader" copy will be replicated to all "follower" replicas, before the
    write operation is reported successful. If a server fails, this is detected
    automatically and one of the servers holding copies take over, usually
    without an error being reported.

  - **satellite**: `Boolean`

    If the true the collection is created as a satellite collection.
    In this case the _replicationFactor_ is ignored.

  - **waitForSync**: `Boolean`

    If true then the data is synchronized to disk before returning from a
    document create, update, replace or removal operation. (default: false)

  - **doCompact**: `Boolean`

    Whether or not the collection will be compacted (default is true)

  - **isVolatile**: `Boolean`

    If true then the collection data is kept in-memory only and not made persistent.
    Unloading the collection will cause the collection data to be discarded.
    Stopping or re-starting the server will also cause full loss of data in
    the collection. Setting this option will make the resulting collection be
    slightly faster than regular collections because ArangoDB does not enforce
    any synchronization to disk and does not calculate any CRC checksums for
    datafiles (as there are no datafiles). This option should therefore be used
    for cache-type collections only, and not for data that cannot be re-created
    otherwise. (The default is false)

  - **shardKeys**: `String...`

    (The default is [ "_key" ]): in a cluster, this attribute determines which
    document attributes are used to determine the target shard for documents.
    Documents are sent to shards based on the values of their shard key attributes.
    The values of all shard key attributes in a document are hashed, and the
    hash value is used to determine the target shard. Note: Values of shard key
    attributes cannot be changed once set. This option is meaningless in a
    single server setup.

  - **numberOfShards**: `Integer`

    (The default is 1): in a cluster, this value determines the number of shards
    to create for the collection. In a single server setup, this option is meaningless.

  - **isSystem**: `Boolean`

    If true, create a system collection. In this case collection-name should
    start with an underscore. End users should normally create non-system
    collections only. API implementors may be required to create system collections
    in very special occasions, but normally a regular collection will do.
    (The default is false)

  - **type**: `CollectionType`

    (The default is _CollectionType#DOCUMENT_): the type of the collection to create.

  - **indexBuckets**: `Integer`

    The number of buckets into which indexes using a hash table are split.
    The default is 16 and this number has to be a power of 2 and less than or
    equal to 1024. For very large collections one should increase this to avoid
    long pauses when the hash table has to be initially built or resized, since
    buckets are resized individually and can be initially built in parallel.
    For example, 64 might be a sensible value for a collection with
    100 000 000 documents. Currently, only the edge index respects this value,
    but other index types might follow in future ArangoDB versions.
    Changes (see below) are applied when the collection is loaded the next time.

  - **distributeShardsLike**: `String`

    (The default is ""): in an Enterprise Edition cluster, this attribute binds
    the specifics of sharding for the newly created collection to follow that
    of a specified existing collection. Note: Using this parameter has
    consequences for the prototype collection. It can no longer be dropped,
    before sharding imitating collections are dropped. Equally, backups and
    restores of imitating collections alone will generate warnings, which can
    be overridden, about missing sharding prototype.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
db.createCollection("potatoes", new CollectionCreateOptions());
// the document collection "potatoes" now exists
```

## ArangoCollection.create

`ArangoCollection.create(CollectionCreateOptions options) : CollectionEntity`

Creates a collection with the given _options_ for this collection's name,
then returns collection information from the server.

Alternative for [ArangoDatabase.createCollection](#arangodatabasecreatecollection).

**Arguments**

- **options**: `CollectionCreateOptions`

  - **journalSize**: `Long`

    The maximal size of a journal or datafile in bytes.
    The value must be at least 1048576 (1 MiB).

  - **replicationFactor**: `Integer`

    (The default is 1): in a cluster, this attribute determines how many copies
    of each shard are kept on different DBServers. The value 1 means that only
    one copy (no synchronous replication) is kept. A value of k means that k-1
    replicas are kept. Any two copies reside on different DBServers.
    Replication between them is synchronous, that is, every write operation to
    the "leader" copy will be replicated to all "follower" replicas, before the
    write operation is reported successful. If a server fails, this is detected
    automatically and one of the servers holding copies take over, usually
    without an error being reported.

  - **satellite**: `Boolean`

    If the true the collection is created as a satellite collection.
    In this case the _replicationFactor_ is ignored.

  - **waitForSync**: `Boolean`

    If true then the data is synchronized to disk before returning from a
    document create, update, replace or removal operation. (default: false)

  - **doCompact**: `Boolean`

    Whether or not the collection will be compacted (default is true)

  - **isVolatile**: `Boolean`

    If true then the collection data is kept in-memory only and not made persistent.
    Unloading the collection will cause the collection data to be discarded.
    Stopping or re-starting the server will also cause full loss of data in
    the collection. Setting this option will make the resulting collection be
    slightly faster than regular collections because ArangoDB does not enforce
    any synchronization to disk and does not calculate any CRC checksums for
    datafiles (as there are no datafiles). This option should therefore be used
    for cache-type collections only, and not for data that cannot be re-created
    otherwise. (The default is false)

  - **shardKeys**: `String...`

    (The default is [ "_key" ]): in a cluster, this attribute determines which
    document attributes are used to determine the target shard for documents.
    Documents are sent to shards based on the values of their shard key attributes.
    The values of all shard key attributes in a document are hashed, and the
    hash value is used to determine the target shard. Note: Values of shard key
    attributes cannot be changed once set. This option is meaningless in a
    single server setup.

  - **numberOfShards**: `Integer`

    (The default is 1): in a cluster, this value determines the number of shards
    to create for the collection. In a single server setup, this option is meaningless.

  - **isSystem**: `Boolean`

    If true, create a system collection. In this case collection-name should
    start with an underscore. End users should normally create non-system
    collections only. API implementors may be required to create system collections
    in very special occasions, but normally a regular collection will do.
    (The default is false)

  - **type**: `CollectionType`

    (The default is _CollectionType#DOCUMENT_): the type of the collection to create.

  - **indexBuckets**: `Integer`

    The number of buckets into which indexes using a hash table are split.
    The default is 16 and this number has to be a power of 2 and less than or
    equal to 1024. For very large collections one should increase this to avoid
    long pauses when the hash table has to be initially built or resized, since
    buckets are resized individually and can be initially built in parallel.
    For example, 64 might be a sensible value for a collection with
    100 000 000 documents. Currently, only the edge index respects this value,
    but other index types might follow in future ArangoDB versions.
    Changes (see below) are applied when the collection is loaded the next time.

  - **distributeShardsLike**: `String`

    (The default is ""): in an Enterprise Edition cluster, this attribute binds
    the specifics of sharding for the newly created collection to follow that
    of a specified existing collection. Note: Using this parameter has
    consequences for the prototype collection. It can no longer be dropped,
    before sharding imitating collections are dropped. Equally, backups and
    restores of imitating collections alone will generate warnings, which can
    be overridden, about missing sharding prototype.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("potatoes");
collection.create(new CollectionCreateOptions());
// the document collection "potatoes" now exists
```

## ArangoCollection.load

`ArangoCollection.load() : CollectionEntity`

Tells the server to load the collection into memory.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");
collection.load();
// the collection has now been loaded into memory
```

## ArangoCollection.unload

`ArangoCollection.unload() : CollectionEntity`

Tells the server to remove the collection from memory. This call does not
delete any documents. You can use the collection afterwards; in which case
it will be loaded into memory, again.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");
collection.unload();
// the collection has now been unloaded from memory
```

## ArangoCollection.changeProperties

`ArangoCollection.changeProperties(CollectionPropertiesOptions options) : CollectionPropertiesEntity`

Changes the properties of the collection.

**Arguments**

- **options**: `CollectionPropertiesEntity`

  For information on the _properties_ argument see
  [the HTTP API for modifying collections](../../../..//HTTP/Collection/Modifying.html).

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

CollectionPropertiesEntity result = collection.changeProperties(
  new CollectionPropertiesEntity().waitForSync(true)
);
assertThat(result.getWaitForSync(), is(true));
// the collection will now wait for data being written to disk
// whenever a document is changed
```

## ArangoCollection.rename

`ArangoCollection.rename(String newName) : CollectionEntity`

Renames the collection

**Arguments**

- **newName**: `String`

  The new name

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

CollectionEntity result = collection.rename("new-collection-name")
assertThat(result.getName(), is("new-collection-name");
// result contains additional information about the collection
```

## ArangoCollection.truncate

`ArangoCollection.truncate() : CollectionEntity`

Removes all documents from the collection, but leaves the indexes intact.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

collection.truncate();
// the collection "some-collection" is now empty
```

## ArangoCollection.drop

`ArangoCollection.drop() : void`

Deletes the collection from the database.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoCollection collection = db.collection("some-collection");

collection.drop();
// the collection "some-collection" no longer exists
```
