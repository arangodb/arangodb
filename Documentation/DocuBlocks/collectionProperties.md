
@startDocuBlock collectionProperties
@brief gets or sets the properties of a collection
`collection.properties()`

Returns an object containing all collection properties.

* *waitForSync*: If *true* creating a document will only return
  after the data was synced to disk.

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

* *schema* (optional, default is *null*): 
  Object that specifies the collection level document schema for documents.
  The attribute keys `rule`, `level` and `message` must follow the rules
  documented in [Document Schema Validation](https://www.arangodb.com/docs/devel/document-schema-validation.html)

In a cluster setup, the result will also contain the following attributes:

* *numberOfShards*: the number of shards of the collection.

* *shardKeys*: contains the names of document attributes that are used to
  determine the target shard for documents.

* *replicationFactor*: determines how many copies of each shard are kept
  on different DB-Servers. Has to be in the range of 1-10 or the string
  `"satellite"` for a SatelliteCollection (Enterprise Edition only).
  _(cluster only)_

* *writeConcern*: determines how many copies of each shard are required to be
  in sync on the different DB-Servers. If there are less then these many copies
  in the cluster a shard will refuse to write. Writes to shards with enough
  up-to-date copies will succeed at the same time however. The value of
  *writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

* *shardingStrategy*: the sharding strategy selected for the collection.
  This attribute will only be populated in cluster mode and is not populated
  in single-server mode. _(cluster only)_

`collection.properties(properties)`

Changes the collection properties. *properties* must be an object with
one or more of the following attribute(s):

* *waitForSync*: If *true* creating a document will only return
  after the data was synced to disk.

* *replicationFactor*: Change the number of shard copies kept on
  different DB-Servers. Valid values are integer numbers in the range of 1-10
  or the string `"satellite"` for a SatelliteCollection (Enterprise Edition only).
  _(cluster only)_

* *writeConcern*: change how many copies of each shard are required to be
  in sync on the different DB-Servers. If there are less then these many copies
  in the cluster a shard will refuse to write. Writes to shards with enough
  up-to-date copies will succeed at the same time however. The value of
  *writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

**Note**: some other collection properties, such as *type*,
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

