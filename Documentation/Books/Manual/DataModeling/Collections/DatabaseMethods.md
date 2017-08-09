Database Methods
================

### Collection
<!-- arangod/V8Server/v8-vocbase.cpp -->


returns a single collection or null
`db._collection(collection-name)`

Returns the collection with the given name or null if no such collection
exists.

`db._collection(collection-identifier)`

Returns the collection with the given identifier or null if no such
collection exists. Accessing collections by identifier is discouraged for
end users. End users should access collections using the collection name.


**Examples**


Get a collection by name:

    @startDocuBlockInline collectionDatabaseNameKnown
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseNameKnown}
      db._collection("demo");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseNameKnown

Get a collection by id:

```
arangosh> db._collection(123456);
[ArangoCollection 123456, "demo" (type document, status loaded)]
```

Unknown collection:

    @startDocuBlockInline collectionDatabaseNameUnknown
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseNameUnknown}
      db._collection("unknown");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseNameUnknown


### Create
<!-- arangod/V8Server/v8-vocindex.cpp -->


creates a new document or edge collection
`db._create(collection-name)`

Creates a new document collection named *collection-name*.
If the collection name already exists or if the name format is invalid, an
error is thrown. For more information on valid collection names please refer
to the [naming conventions](../NamingConventions/README.md).

`db._create(collection-name, properties)`

*properties* must be an object with the following attributes:

- *waitForSync* (optional, default *false*): If *true* creating
  a document will only return after the data was synced to disk.

- *journalSize* (optional, default is a
  configuration parameter: The maximal
  size of a journal or datafile.  Note that this also limits the maximal
  size of a single object. Must be at least 1MB.

- *isSystem* (optional, default is *false*): If *true*, create a
  system collection. In this case *collection-name* should start with
  an underscore. End users should normally create non-system collections
  only. API implementors may be required to create system collections in
  very special occasions, but normally a regular collection will do.

- *isVolatile* (optional, default is *false*): If *true* then the
  collection data is kept in-memory only and not made persistent. Unloading
  the collection will cause the collection data to be discarded. Stopping
  or re-starting the server will also cause full loss of data in the
  collection. The collection itself will remain however (only the data is
  volatile). Setting this option will make the resulting collection be
  slightly faster than regular collections because ArangoDB does not
  enforce any synchronization to disk and does not calculate any CRC
  checksums for datafiles (as there are no datafiles).
  This option is meaningful for the MMFiles storage engine only.

- *keyOptions* (optional): additional options for key generation. If
  specified, then *keyOptions* should be a JSON object containing the
  following attributes (**note**: some of them are optional):
  - *type*: specifies the type of the key generator. The currently
    available generators are *traditional* and *autoincrement*.
    (**note**: *autoincrement* is currently only supported for non-sharded
    collections)
  - *allowUserKeys*: if set to *true*, then it is allowed to supply
    own key values in the *_key* attribute of a document. If set to
    *false*, then the key generator will solely be responsible for
    generating keys and supplying own key values in the *_key* attribute
    of documents is considered an error.
  - *increment*: increment value for *autoincrement* key generator.
    Not used for other key generator types.
  - *offset*: initial offset value for *autoincrement* key generator.
    Not used for other key generator types.

- *numberOfShards* (optional, default is *1*): in a cluster, this value
  determines the number of shards to create for the collection. In a single
  server setup, this option is meaningless.

- *shardKeys* (optional, default is `[ "_key" ]`): in a cluster, this
  attribute determines which document attributes are used to determine the
  target shard for documents. Documents are sent to shards based on the
  values they have in their shard key attributes. The values of all shard
  key attributes in a document are hashed, and the hash value is used to
  determine the target shard. Note that values of shard key attributes cannot
  be changed once set.
  This option is meaningless in a single server setup.

  When choosing the shard keys, one must be aware of the following
  rules and limitations: In a sharded collection with more than
  one shard it is not possible to set up a unique constraint on
  an attribute that is not the one and only shard key given in
  *shardKeys*. This is because enforcing a unique constraint
  would otherwise make a global index necessary or need extensive
  communication for every single write operation. Furthermore, if
  *_key* is not the one and only shard key, then it is not possible
  to set the *_key* attribute when inserting a document, provided
  the collection has more than one shard. Again, this is because
  the database has to enforce the unique constraint on the *_key*
  attribute and this can only be done efficiently if this is the
  only shard key by delegating to the individual shards.

- *replicationFactor* (optional, default is 1): in a cluster, this
  attribute determines how many copies of each shard are kept on 
  different DBServers. The value 1 means that only one copy (no
  synchronous replication) is kept. A value of k means that
  k-1 replicas are kept. Any two copies reside on different DBServers.
  Replication between them is synchronous, that is, every write operation
  to the "leader" copy will be replicated to all "follower" replicas,
  before the write operation is reported successful.

  If a server fails, this is detected automatically and one of the
  servers holding copies take over, usually without an error being
  reported.

- *distributeShardsLike* distribute the shards of this collection
  cloning the shard distribution of another.

  When using the *Enterprise* version of ArangoDB the replicationFactor
  may be set to "satellite" making the collection locally joinable
  on every database server. This reduces the number of network hops
  dramatically when using joins in AQL at the costs of reduced write
  performance on these collections.


`db._create(collection-name, properties, type)`

Specifies the optional *type* of the collection, it can either be *document* 
or *edge*. On default it is document. Instead of giving a type you can also use 
*db._createEdgeCollection* or *db._createDocumentCollection*.


**Examples**


With defaults:

    @startDocuBlockInline collectionDatabaseCreateSuccess
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateSuccess}
      c = db._create("users");
      c.properties();
    ~ db._drop("users");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseCreateSuccess

With properties:

    @startDocuBlockInline collectionDatabaseCreateProperties
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateProperties}
      |c = db._create("users", { waitForSync : true,
               journalSize : 1024 * 1204});
      c.properties();
    ~ db._drop("users");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseCreateProperties

With a key generator:

    @startDocuBlockInline collectionDatabaseCreateKey
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateKey}
    | db._create("users",
         { keyOptions: { type: "autoincrement", offset: 10, increment: 5 } });
      db.users.save({ name: "user 1" });
      db.users.save({ name: "user 2" });
      db.users.save({ name: "user 3" });
    ~ db._drop("users");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseCreateKey

With a special key option:

    @startDocuBlockInline collectionDatabaseCreateSpecialKey
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateSpecialKey}
      db._create("users", { keyOptions: { allowUserKeys: false } });
      db.users.save({ name: "user 1" });
      db.users.save({ name: "user 2", _key: "myuser" }); // xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)
      db.users.save({ name: "user 3" });
    ~ db._drop("users");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseCreateSpecialKey


<!-- arangod/V8Server/v8-vocindex.cpp -->


creates a new edge collection
`db._createEdgeCollection(collection-name)`

Creates a new edge collection named *collection-name*. If the
collection name already exists an error is thrown. The default value
for *waitForSync* is *false*.

`db._createEdgeCollection(collection-name, properties)`

*properties* must be an object with the following attributes:

- *waitForSync* (optional, default *false*): If *true* creating
  a document will only return after the data was synced to disk.
- *journalSize* (optional, default is 
  "configuration parameter"):  The maximal size of
  a journal or datafile.  Note that this also limits the maximal
  size of a single object and must be at least 1MB.



<!-- arangod/V8Server/v8-vocindex.cpp -->


creates a new document collection
`db._createDocumentCollection(collection-name)`

Creates a new document collection named *collection-name*. If the
document name already exists and error is thrown.


### All Collections
<!-- arangod/V8Server/v8-vocbase.cpp -->


returns all collections
`db._collections()`

Returns all collections of the given database.


**Examples**


    @startDocuBlockInline collectionsDatabaseName
    @EXAMPLE_ARANGOSH_OUTPUT{collectionsDatabaseName}
    ~ db._create("example");
      db._collections();
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionsDatabaseName



### Collection Name
<!-- arangod/V8Server/v8-vocbase.cpp -->


selects a collection from the vocbase
`db.collection-name`

Returns the collection with the given *collection-name*. If no such
collection exists, create a collection named *collection-name* with the
default properties.


**Examples**


    @startDocuBlockInline collectionDatabaseCollectionName
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCollectionName}
    ~ db._create("example");
      db.example;
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseCollectionName



### Drop
<!-- js/server/modules/@arangodb/arango-database.js -->


drops a collection
`db._drop(collection)`

Drops a *collection* and all its indexes and data.

`db._drop(collection-identifier)`

Drops a collection identified by *collection-identifier* with all its
indexes and data. No error is thrown if there is no such collection.

`db._drop(collection-name)`

Drops a collection named *collection-name* and all its indexes. No error
is thrown if there is no such collection.

`db._drop(collection-name, options)`

In order to drop a system collection, one must specify an *options* object
with attribute *isSystem* set to *true*. Otherwise it is not possible to
drop system collections.

**Note**: cluster collection, which are prototypes for collections
with *distributeShardsLike* parameter, cannot be dropped.

*Examples*

Drops a collection:

    @startDocuBlockInline collectionDatabaseDropByObject
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDropByObject}
    ~ db._create("example");
      col = db.example;
      db._drop(col);
      col;
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseDropByObject

Drops a collection identified by name:

    @startDocuBlockInline collectionDatabaseDropName
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDropName}
    ~ db._create("example");
      col = db.example;
      db._drop("example");
      col;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseDropName

Drops a system collection

    @startDocuBlockInline collectionDatabaseDropSystem
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseDropSystem}
    ~ db._create("_example", { isSystem: true });
      col = db._example;
      db._drop("_example", { isSystem: true });
      col;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseDropSystem

### Truncate
<!-- js/server/modules/@arangodb/arango-database.js -->


truncates a collection
`db._truncate(collection)`

Truncates a *collection*, removing all documents but keeping all its
indexes.

`db._truncate(collection-identifier)`

Truncates a collection identified by *collection-identified*. No error is
thrown if there is no such collection.

`db._truncate(collection-name)`

Truncates a collection named *collection-name*. No error is thrown if
there is no such collection.


**Examples**


Truncates a collection:

    @startDocuBlockInline collectionDatabaseTruncateByObject
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncateByObject}
    ~ db._create("example");
      col = db.example;
      col.save({ "Hello" : "World" });
      col.count();
      db._truncate(col);
      col.count();
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseTruncateByObject

Truncates a collection identified by name:

    @startDocuBlockInline collectionDatabaseTruncateName
    @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseTruncateName}
    ~ db._create("example");
      col = db.example;
      col.save({ "Hello" : "World" });
      col.count();
      db._truncate("example");
      col.count();
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock collectionDatabaseTruncateName


