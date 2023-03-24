
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
If `true` then the data is synchronized to disk before returning from a
document create, update, replace or removal operation. (Default: `false`)

@RESTBODYPARAM{isSystem,boolean,optional,}
If `true`, create a system collection. In this case, the `collection-name`
should start with an underscore. End-users should normally create non-system
collections only. API implementors may be required to create system
collections in very special occasions, but normally a regular collection will do.
(The default is `false`)

@RESTBODYPARAM{schema,object,optional,}
Optional object that specifies the collection level schema for
documents. The attribute keys `rule`, `level` and `message` must follow the
rules documented in [Document Schema Validation](https://www.arangodb.com/docs/stable/documents-schema-validation.html)

@RESTBODYPARAM{computedValues,array,optional,post_api_collection_computed_field}
An optional list of objects, each representing a computed value.

@RESTSTRUCT{name,post_api_collection_computed_field,string,required,}
The name of the target attribute. Can only be a top-level attribute, but you
may return a nested object. Cannot be `_key`, `_id`, `_rev`, `_from`, `_to`,
or a shard key attribute.

@RESTSTRUCT{expression,post_api_collection_computed_field,string,required,}
An AQL `RETURN` operation with an expression that computes the desired value.
See [Computed Value Expressions](https://www.arangodb.com/docs/devel/data-modeling-documents-computed-values.html#computed-value-expressions) for details.

@RESTSTRUCT{overwrite,post_api_collection_computed_field,boolean,required,}
Whether the computed value shall take precedence over a user-provided or
existing attribute.

@RESTSTRUCT{computeOn,post_api_collection_computed_field,array,optional,string}
An array of strings to define on which write operations the value shall be
computed. The possible values are `"insert"`, `"update"`, and `"replace"`.
The default is `["insert", "update", "replace"]`.

@RESTSTRUCT{keepNull,post_api_collection_computed_field,boolean,optional,}
Whether the target attribute shall be set if the expression evaluates to `null`.
You can set the option to `false` to not set (or unset) the target attribute if
the expression returns `null`. The default is `true`.

@RESTSTRUCT{failOnWarning,post_api_collection_computed_field,boolean,optional,}
Whether to let the write operation fail if the expression produces a warning.
The default is `false`.

@RESTBODYPARAM{keyOptions,object,optional,post_api_collection_opts}
additional options for key generation. If specified, then `keyOptions`
should be a JSON object containing the following attributes:

@RESTSTRUCT{type,post_api_collection_opts,string,required,string}
specifies the type of the key generator. The currently available generators are
`traditional`, `autoincrement`, `uuid` and `padded`.

- The `traditional` key generator generates numerical keys in ascending order.
  The sequence of keys is not guaranteed to be gap-free.

- The `autoincrement` key generator generates numerical keys in ascending order,
  the initial offset and the spacing can be configured (**note**: `autoincrement`
  is currently only supported for non-sharded collections).
  The sequence of generated keys is not guaranteed to be gap-free, because a new key
  will be generated on every document insert attempt, not just for successful
  inserts.

- The `padded` key generator generates keys of a fixed length (16 bytes) in
  ascending lexicographical sort order. This is ideal for usage with the _RocksDB_
  engine, which will slightly benefit keys that are inserted in lexicographically
  ascending order. The key generator can be used in a single-server or cluster.
  The sequence of generated keys is not guaranteed to be gap-free.

- The `uuid` key generator generates universally unique 128 bit keys, which
  are stored in hexadecimal human-readable format. This key generator can be used
  in a single-server or cluster to generate "seemingly random" keys. The keys
  produced by this key generator are not lexicographically sorted.

Please note that keys are only guaranteed to be truly ascending in single
server deployments and for collections that only have a single shard (that includes
collections in a OneShard database).
The reason is that for collections with more than a single shard, document keys
are generated on coordinator(s). For collections with a single shard, the document
keys are generated on the leader DB server, which has full control over the key
sequence.

@RESTSTRUCT{allowUserKeys,post_api_collection_opts,boolean,required,}
If set to `true`, then you are allowed to supply own key values in the
`_key` attribute of documents. If set to `false`, then the key generator
is solely be responsible for generating keys and an error is raised if you
supply own key values in the `_key` attribute of documents.

@RESTSTRUCT{increment,post_api_collection_opts,integer,required,int64}
increment value for `autoincrement` key generator. Not used for other key
generator types.

@RESTSTRUCT{offset,post_api_collection_opts,integer,required,int64}
Initial offset value for `autoincrement` key generator.
Not used for other key generator types.

@RESTBODYPARAM{type,integer,optional,int64}
(The default is `2`): the type of the collection to create.
The following values for `type` are valid:

- `2`: document collection
- `3`: edge collection

@RESTBODYPARAM{cacheEnabled,boolean,optional,}
Whether the in-memory hash cache for documents should be enabled for this
collection (default: `false`). Can be controlled globally with the `--cache.size`
startup option. The cache can speed up repeated reads of the same documents via
their document keys. If the same documents are not fetched often or are
modified frequently, then you may disable the cache to avoid the maintenance
costs.

@RESTBODYPARAM{numberOfShards,integer,optional,int64}
(The default is `1`): in a cluster, this value determines the
number of shards to create for the collection. In a single
server setup, this option is meaningless.

@RESTBODYPARAM{shardKeys,string,optional,string}
(The default is `[ "_key" ]`): in a cluster, this attribute determines
which document attributes are used to determine the target shard for documents.
Documents are sent to shards based on the values of their shard key attributes.
The values of all shard key attributes in a document are hashed,
and the hash value is used to determine the target shard.
**Note**: Values of shard key attributes cannot be changed once set.
  This option is meaningless in a single server setup.

@RESTBODYPARAM{replicationFactor,integer,optional,int64}
(The default is `1`): in a cluster, this attribute determines how many copies
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
in sync on the different DB-Servers. If there are less than these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
`writeConcern` cannot be larger than `replicationFactor`. _(cluster only)_

@RESTBODYPARAM{shardingStrategy,string,optional,string}
This attribute specifies the name of the sharding strategy to use for
the collection. Since ArangoDB 3.4 there are different sharding strategies
to select from when creating a new collection. The selected `shardingStrategy`
value remains fixed for the collection and cannot be changed afterwards.
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
- `enterprise-hex-smart-vertex`: sharding used for vertex collections of
  EnterpriseGraphs

If no sharding strategy is specified, the default is `hash` for
all normal collections, `enterprise-hash-smart-edge` for all smart edge
collections, and `enterprise-hex-smart-vertex` for EnterpriseGraph
vertex collections (the latter two require the *Enterprise Edition* of ArangoDB).
Manually overriding the sharding strategy does not yet provide a
benefit, but it may later in case other sharding strategies are added.

@RESTBODYPARAM{distributeShardsLike,string,optional,string}
The name of another collection. If this property is set in a cluster, the
collection copies the `replicationFactor`, `numberOfShards` and `shardingStrategy`
properties from the specified collection (referred to as the _prototype collection_)
and distributes the shards of this collection in the same way as the shards of
the other collection. In an Enterprise Edition cluster, this data co-location is
utilized to optimize queries.

You need to use the same number of `shardKeys` as the prototype collection, but
you can use different attributes.

The default is `""`.

**Note**: Using this parameter has consequences for the prototype
collection. It can no longer be dropped, before the sharding-imitating
collections are dropped. Equally, backups and restores of imitating
collections alone generate warnings (which can be overridden)
about a missing sharding prototype.

@RESTBODYPARAM{isSmart,boolean,optional,}
Whether the collection is for a SmartGraph or EnterpriseGraph
(Enterprise Edition only). This is an internal property.

@RESTBODYPARAM{isDisjoint,boolean,optional,}
Whether the collection is for a Disjoint SmartGraph
(Enterprise Edition only). This is an internal property.

@RESTBODYPARAM{smartGraphAttribute,string,optional,string}
The attribute that is used for sharding: vertices with the same value of
this attribute are placed in the same shard. All vertices are required to
have this attribute set and it has to be a string. Edges derive the
attribute from their connected vertices.

This feature can only be used in the *Enterprise Edition*.

@RESTBODYPARAM{smartJoinAttribute,string,optional,string}
In an *Enterprise Edition* cluster, this attribute determines an attribute
of the collection that must contain the shard key value of the referred-to
SmartJoin collection. Additionally, the shard key for a document in this
collection must contain the value of this attribute, followed by a colon,
followed by the actual primary key of the document.

This feature can only be used in the *Enterprise Edition* and requires the
`distributeShardsLike` attribute of the collection to be set to the name
of another collection. It also requires the `shardKeys` attribute of the
collection to be set to a single shard key attribute, with an additional ':'
at the end.
A further restriction is that whenever documents are stored or updated in the
collection, the value stored in the `smartJoinAttribute` must be a string.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSyncReplication,boolean,optional}
The default is `true`, which means the server only reports success back to the
client when all replicas have created the collection. Set it to `false` if you want
faster server responses and don't care about full replication.

@RESTQUERYPARAM{enforceReplicationFactor,boolean,optional}
The default is `true`, which means the server checks if there are enough replicas
available at creation time and bail out otherwise. Set it to `false` to disable
this extra check.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the `collection-name` is missing, then an *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the `collection-name` is unknown, then an *HTTP 404* is returned.

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
