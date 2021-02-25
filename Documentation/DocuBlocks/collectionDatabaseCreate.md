

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

* *isSystem* (optional, default is *false*): If *true*, create a
  system collection. In this case *collection-name* should start with
  an underscore. End users should normally create non-system collections
  only. API implementors may be required to create system collections in
  very special occasions, but normally a regular collection will do.

* *keyOptions* (optional): additional options for key generation. If
  specified, then *keyOptions* should be a JSON array containing the
  following attributes (**note**: some of them are optional):
  * *type*: specifies the type of the key generator. The currently
    available generators are *traditional*, *autoincrement*, *uuid*
    and *padded*.

    The *traditional* key generator generates numerical keys in ascending order.
    The *autoincrement* key generator generates numerical keys in ascending order, 
    the inital offset and the spacing can be configured (**note**: *autoincrement* is currently only 
    supported for non-sharded collections). 
    The *padded* key generator generates keys of a fixed length (16 bytes) in
    ascending lexicographical sort order. This is ideal for usage with the _RocksDB_
    engine, which will slightly benefit keys that are inserted in lexicographically
    ascending order. The key generator can be used in a single-server or cluster.
    The *uuid* key generator generates universally unique 128 bit keys, which 
    are stored in hexadecimal human-readable format. This key generator can be used
    in a single-server or cluster to generate "seemingly random" keys. The keys 
    produced by this key generator are not lexicographically sorted.

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

 *cacheEnabled* (optional, default is *false*, **rocksdb-only**, from v.3.4): Enable in-memory
  caching for documents and primary index entries. This can potentially speed up point-lookups significantly,
  especially if your collection has a subset of frequently accessed keys. Please test this feature
  carefully to ensure that it does not adversely affect the performance of your system.

* *schema* (optional, default is *null*): 
  Object that specifies the collection level schema for documents.
  The attribute keys `rule`, `level` and `message` must follow the rules
  documented in [Document Schema Validation](https://www.arangodb.com/docs/devel/document-schema-validation.html)

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
  c = db._create("users", { waitForSync: true });
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

