
@startDocuBlock post_api_collection
@brief creates a collection

@RESTHEADER{POST /_api/collection, Create collection, handleCommandPost:CreateCollection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTDESCRIPTION
Creates a new collection with a given name. The request must contain an
object with the following attributes.

@RESTBODYPARAM{name,string,required,string}
The name of the collection.

@RESTBODYPARAM{waitForSync,boolean,optional,}
If *true* then the data is synchronized to disk before returning from a
document create, update, replace or removal operation. (default: false)

@RESTBODYPARAM{doCompact,boolean,optional,}
whether or not the collection will be compacted (default is *true*)
This option is meaningful for the MMFiles storage engine only.

@RESTBODYPARAM{journalSize,integer,optional,int64}
The maximal size of a journal or datafile in bytes. The value
must be at least `1048576` (1 MiB). (The default is a configuration parameter)
This option is meaningful for the MMFiles storage engine only.

@RESTBODYPARAM{isSystem,boolean,optional,}
If *true*, create a  system collection. In this case *collection-name*
should start with an underscore. End users should normally create non-system
collections only. API implementors may be required to create system
collections in very special occasions, but normally a regular collection will do.
(The default is *false*)

@RESTBODYPARAM{isVolatile,boolean,optional,}
If *true* then the collection data is kept in-memory only and not made persistent.
Unloading the collection will cause the collection data to be discarded. Stopping
or re-starting the server will also cause full loss of data in the
collection. Setting this option will make the resulting collection be
slightly faster than regular collections because ArangoDB does not
enforce any synchronization to disk and does not calculate any CRC
checksums for datafiles (as there are no datafiles). This option
should therefore be used for cache-type collections only, and not
for data that cannot be re-created otherwise.
(The default is *false*)
This option is meaningful for the MMFiles storage engine only.

@RESTBODYPARAM{schema,object,optional,}
Optional object that specifies the collection level schema for
documents. The attribute keys `rule`, `level` and `message` must follow the
rules documented in [Document Schema Validation](https://www.arangodb.com/docs/devel/document-schema-validation.html)

@RESTBODYPARAM{keyOptions,object,optional,post_api_collection_opts}
additional options for key generation. If specified, then *keyOptions*
should be a JSON array containing the following attributes:

@RESTSTRUCT{type,post_api_collection_opts,string,required,string}
specifies the type of the key generator. The currently available generators are
*traditional*, *autoincrement*, *uuid* and *padded*.<br>
The *traditional* key generator generates numerical keys in ascending order.
The *autoincrement* key generator generates numerical keys in ascending order,
the initial offset and the spacing can be configured
The *padded* key generator generates keys of a fixed length (16 bytes) in
ascending lexicographical sort order. This is ideal for usage with the _RocksDB_
engine, which will slightly benefit keys that are inserted in lexicographically
ascending order. The key generator can be used in a single-server or cluster.
The *uuid* key generator generates universally unique 128 bit keys, which
are stored in hexadecimal human-readable format. This key generator can be used
in a single-server or cluster to generate "seemingly random" keys. The keys
produced by this key generator are not lexicographically sorted.

@RESTSTRUCT{allowUserKeys,post_api_collection_opts,boolean,required,}
if set to *true*, then it is allowed to supply own key values in the
*_key* attribute of a document. If set to *false*, then the key generator
will solely be responsible for generating keys and supplying own key values
in the *_key* attribute of documents is considered an error.

@RESTSTRUCT{increment,post_api_collection_opts,integer,required,int64}
increment value for *autoincrement* key generator. Not used for other key
generator types.

@RESTSTRUCT{offset,post_api_collection_opts,integer,required,int64}
Initial offset value for *autoincrement* key generator.
Not used for other key generator types.

@RESTBODYPARAM{type,integer,optional,int64}
(The default is *2*): the type of the collection to create.
The following values for *type* are valid:<br>
- *2*: document collection
- *3*: edge collection

@RESTBODYPARAM{numberOfShards,integer,optional,int64}
(The default is *1*): in a cluster, this value determines the
number of shards to create for the collection. In a single
server setup, this option is meaningless.

@RESTBODYPARAM{shardKeys,string,optional,string}
(The default is *[ "_key" ]*): in a cluster, this attribute determines
which document attributes are used to determine the target shard for documents.
Documents are sent to shards based on the values of their shard key attributes.
The values of all shard key attributes in a document are hashed,
and the hash value is used to determine the target shard.
**Note**: Values of shard key attributes cannot be changed once set.
  This option is meaningless in a single server setup.

@RESTBODYPARAM{replicationFactor,integer,optional,int64}
(The default is *1*): in a cluster, this attribute determines how many copies
of each shard are kept on different DB-Servers. The value 1 means that only one
copy (no synchronous replication) is kept. A value of k means that k-1 replicas
are kept. It can also be the string `"satellite"` for a SatelliteCollection,
where the replication factor is matched to the number of DB-Servers
(Enterprise Edition only).

Any two copies reside on different DB-Servers. Replication between them is
synchronous, that is, every write operation to the "leader" copy will be replicated
to all "follower" replicas, before the write operation is reported successful.

If a server fails, this is detected automatically and one of the servers holding
copies take over, usually without an error being reported.

@RESTBODYPARAM{writeConcern,integer,optional,int64}
Write concern for this collection (default: 1).
It determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
*writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

@RESTBODYPARAM{distributeShardsLike,string,optional,string}
(The default is *""*): in an Enterprise Edition cluster, this attribute binds
the specifics of sharding for the newly created collection to follow that of a
specified existing collection.
**Note**: Using this parameter has consequences for the prototype
collection. It can no longer be dropped, before the sharding-imitating
collections are dropped. Equally, backups and restores of imitating
collections alone will generate warnings (which can be overridden)
about missing sharding prototype.

@RESTBODYPARAM{shardingStrategy,string,optional,string}
This attribute specifies the name of the sharding strategy to use for
the collection. Since ArangoDB 3.4 there are different sharding strategies
to select from when creating a new collection. The selected *shardingStrategy*
value will remain fixed for the collection and cannot be changed afterwards.
This is important to make the collection keep its sharding settings and
always find documents already distributed to shards using the same
initial sharding algorithm.

The available sharding strategies are:
- `community-compat`: default sharding used by ArangoDB
  Community Edition before version 3.4
- `enterprise-compat`: default sharding used by ArangoDB
  Enterprise Edition before version 3.4
- `enterprise-smart-edge-compat`: default sharding used by smart edge
  collections in ArangoDB Enterprise Edition before version 3.4
- `hash`: default sharding used for new collections starting from version 3.4
  (excluding smart edge collections)
- `enterprise-hash-smart-edge`: default sharding used for new
  smart edge collections starting from version 3.4

If no sharding strategy is specified, the default will be *hash* for
all collections, and *enterprise-hash-smart-edge* for all smart edge
collections (requires the *Enterprise Edition* of ArangoDB).
Manually overriding the sharding strategy does not yet provide a
benefit, but it may later in case other sharding strategies are added.

@RESTBODYPARAM{smartJoinAttribute,string,optional,string}
In an *Enterprise Edition* cluster, this attribute determines an attribute
of the collection that must contain the shard key value of the referred-to
SmartJoin collection. Additionally, the shard key for a document in this
collection must contain the value of this attribute, followed by a colon,
followed by the actual primary key of the document.

This feature can only be used in the *Enterprise Edition* and requires the
*distributeShardsLike* attribute of the collection to be set to the name
of another collection. It also requires the *shardKeys* attribute of the
collection to be set to a single shard key attribute, with an additional ':'
at the end.
A further restriction is that whenever documents are stored or updated in the
collection, the value stored in the *smartJoinAttribute* must be a string.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSyncReplication,integer,optional}
Default is *1* which means the server will only report success back to the
client if all replicas have created the collection. Set to *0* if you want
faster server responses and don't care about full replication.

@RESTQUERYPARAM{enforceReplicationFactor,integer,optional}
Default is *1* which means the server will check if there are enough replicas
available at creation time and bail out otherwise. Set to *0* to disable
this extra check.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@RESTRETURNCODE{200}

@RESTREPLYBODY{,object,required,collection_info}

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionCreateCollection}
    var url = "/_api/collection";
    var body = {
      name: "testCollectionBasics"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      name: "testCollectionEdges",
      type : 3
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);
    logJsonResponse(response);

    db._flushCache();
    db._drop("testCollectionBasics");
    db._drop("testCollectionEdges");
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestCollectionCreateKeyopt}
    var url = "/_api/collection";
    var body = {
      name: "testCollectionUsers",
      keyOptions : {
        type : "autoincrement",
        increment : 5,
        allowUserKeys : true
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);
    logJsonResponse(response);

    db._flushCache();
    db._drop("testCollectionUsers");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
