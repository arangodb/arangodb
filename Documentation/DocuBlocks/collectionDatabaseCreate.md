

@brief creates a new document or edge collection
`db._create(collection-name)`

Creates a new document collection named *collection-name*.
If the collection name already exists or if the name format is invalid, an
error is thrown. For more information on valid collection names please refer
to the [naming conventions](../NamingConventions/README.md).

`db._create(collection-name, properties)`

*properties* must be an object with the following attributes:

* *waitForSync* (optional, default *false*): If *true* creating
  a document will only return after the data was synced to disk.

* *journalSize* (optional, default is a
  global config parameter, **mmfiles-only**): The maximal
  size of a journal or datafile.  Note that this also limits the maximal
  size of a single object. Must be at least 1MB.
  This option is meaningful for the MMFiles storage engine only.

* *isSystem* (optional, default is *false*): If *true*, create a
  system collection. In this case *collection-name* should start with
  an underscore. End users should normally create non-system collections
  only. API implementors may be required to create system collections in
  very special occasions, but normally a regular collection will do.

* *keyOptions* (optional): additional options for key generation. If
  specified, then *keyOptions* should be a JSON array containing the
  following attributes (**note**: some of them are optional):
  * *type*: specifies the type of the key generator. The currently
    available generators are *traditional* and *autoincrement*.
  * *allowUserKeys*: if set to *true*, then it is allowed to supply
    own key values in the *_key* attribute of a document. If set to
    *false*, then the key generator will solely be responsible for
    generating keys and supplying own key values in the *_key* attribute
    of documents is considered an error.
  * *increment*: increment value for *autoincrement* key generator.
    Not used for other key generator types.
  * *offset*: initial offset value for *autoincrement* key generator.
    Not used for other key generator types.

* *numberOfShards* (optional, default is *1*): in a cluster, this value
  determines the number of shards to create for the collection. In a single
  server setup, this option is meaningless.

* *shardKeys* (optional, default is *[ "_key" ]*): in a cluster, this
  attribute determines which document attributes are used to determine the
  target shard for documents. Documents are sent to shards based on the
  values they have in their shard key attributes. The values of all shard
  key attributes in a document are hashed, and the hash value is used to
  determine the target shard. Note that values of shard key attributes cannot
  be changed once set. This option is meaningless in a single server setup.

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

<!---
 *cacheEnabled* (optional, default is *false*, **rocksdb-only**): Enable in-memory
 caching for documents. This can potentially speed up point-lookups significantly,
  especially if your collections has a subset of frequently accessed keys.
-->

* *isVolatile* (optional, default is *false*, **mmfiles-only**): If *true* then the
  collection data is kept in-memory only and not made persistent. Unloading
  the collection will cause the collection data to be discarded. Stopping
  or re-starting the server will also cause full loss of data in the
  collection. Setting this option will make the resulting collection be
  slightly faster than regular collections because ArangoDB does not
  enforce any synchronization to disk and does not calculate any CRC
  checksums for datafiles (as there are no datafiles).
  This option is meaningful for the MMFiles storage engine only.

* *indexBuckets* (optional, default is *16*, **mmfiles-only**): The number of buckets 
  into which indexes using a hash table are split. The default is 16 and 
  this number has to be a power of 2 and less than or equal to 1024. 

  For very large collections one should increase this to avoid long pauses 
  when the hash table has to be initially built or resized, since buckets 
  are resized individually and can be initially built in parallel. For 
  example, 64 might be a sensible value for a collection with 100
  000 000 documents. Currently, only the edge index respects this
  value, but other index types might follow in future ArangoDB versions. 
  Changes (see below) are applied when the collection is loaded the next 
  time.
  This option is meaningful for the MMFiles storage engine only.

`db._create(collection-name, properties, type)`

Specifies the optional *type* of the collection, it can either be *document* 
or *edge*. On default it is document. Instead of giving a type you can also use 
*db._createEdgeCollection* or *db._createDocumentCollection*.

@EXAMPLES

With defaults:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreate}
  c = db._create("users");
  c.properties();
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT

With properties:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateProperties}
  |c = db._create("users", { waitForSync : true,
           journalSize : 1024 * 1204});
  c.properties();
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT

With a key generator:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateKey}
| db._create("users",
     { keyOptions: { type: "autoincrement", offset: 10, increment: 5 } });
  db.users.save({ name: "user 1" });
  db.users.save({ name: "user 2" });
  db.users.save({ name: "user 3" });
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT

With a special key option:

@EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateSpecialKey}
  db._create("users", { keyOptions: { allowUserKeys: false } });
  db.users.save({ name: "user 1" });
| db.users.save({ name: "user 2", _key: "myuser" });
~    // xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)
  db.users.save({ name: "user 3" });
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT

