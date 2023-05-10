@RESTSTRUCT{waitForSync,collection_info,boolean,required,}
If `true`, creating, changing, or removing
documents waits until the data has been synchronized to disk.

@RESTSTRUCT{schema,collection_info,object,optional,}
An object that specifies the collection-level schema for documents.

@RESTSTRUCT{computedValues,collection_info,array,optional,computed_field}
A list of objects, each representing a computed value.

@RESTSTRUCT{name,computed_field,string,required,}
The name of the target attribute.

@RESTSTRUCT{expression,computed_field,string,required,}
An AQL `RETURN` operation with an expression that computes the desired value.

@RESTSTRUCT{overwrite,computed_field,boolean,required,}
Whether the computed value takes precedence over a user-provided or
existing attribute.

@RESTSTRUCT{computeOn,computed_field,array,optional,string}
An array of strings that defines on which write operations the value is
computed. The possible values are `"insert"`, `"update"`, and `"replace"`.

@RESTSTRUCT{keepNull,computed_field,boolean,optional,}
Whether the target attribute is set if the expression evaluates to `null`.

@RESTSTRUCT{failOnWarning,computed_field,boolean,optional,}
Whether the write operation fails if the expression produces a warning.

@RESTSTRUCT{keyOptions,collection_info,object,required,key_generator_type}
An object which contains key generation options.

@RESTSTRUCT{type,key_generator_type,string,required,}
Specifies the type of the key generator. Possible values:
- `"traditional"`
- `"autoincrement"`
- `"uuid"`
- `"padded"`

@RESTSTRUCT{allowUserKeys,key_generator_type,boolean,required,}
If set to `true`, then you are allowed to supply
own key values in the `_key` attribute of a document. If set to
`false`, then the key generator is solely responsible for
generating keys and an error is raised if you supply own key values in the
`_key` attribute of documents.

@RESTSTRUCT{increment,key_generator_type,integer,optional,}
The increment value for the `autoincrement` key generator.
Not used for other key generator types.

@RESTSTRUCT{offset,key_generator_type,integer,optional,}
The initial offset value for the `autoincrement` key generator.
Not used for other key generator types.

@RESTSTRUCT{lastValue,key_generator_type,integer,required,}
The current offset value of the `autoincrement` or `padded` key generator.
This is an internal property for restoring dumps properly.

@RESTSTRUCT{cacheEnabled,collection_info,boolean,required,}
Whether the in-memory hash cache for documents is enabled for this
collection.

@RESTSTRUCT{numberOfShards,collection_info,integer,optional,}
The number of shards of the collection. _(cluster only)_

@RESTSTRUCT{shardKeys,collection_info,array,optional,string}
Contains the names of document attributes that are used to
determine the target shard for documents. _(cluster only)_

@RESTSTRUCT{replicationFactor,collection_info,integer,optional,}
Contains how many copies of each shard are kept on different DB-Servers.
It is an integer number in the range of 1-10 or the string `"satellite"`
for SatelliteCollections (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{writeConcern,collection_info,integer,optional,}
Determines how many copies of each shard are required to be
in-sync on the different DB-Servers. If there are less than these many copies
in the cluster, a shard refuses to write. Writes to shards with enough
up-to-date copies succeed at the same time, however. The value of
`writeConcern` cannot be greater than `replicationFactor`.
For SatelliteCollections, the `writeConcern` is automatically controlled to
equal the number of DB-Servers and has a value of `0`. _(cluster only)_

@RESTSTRUCT{shardingStrategy,collection_info,string,optional,}
The sharding strategy selected for the collection. _(cluster only)_

Possible values:
- `"community-compat"`
- `"enterprise-compat"`
- `"enterprise-smart-edge-compat"`
- `"hash"`
- `"enterprise-hash-smart-edge"`
- `"enterprise-hex-smart-vertex"`

@RESTSTRUCT{distributeShardsLike,collection_info,string,optional,string}
The name of another collection. This collection uses the `replicationFactor`,
`numberOfShards` and `shardingStrategy` properties of the other collection and
the shards of this collection are distributed in the same way as the shards of
the other collection.

@RESTSTRUCT{isSmart,collection_info,boolean,optional,}
Whether the collection is used in a SmartGraph or EnterpriseGraph (Enterprise Edition only).
This is an internal property. _(cluster only)_

@RESTSTRUCT{isDisjoint,collection_info,boolean,optional,}
Whether the SmartGraph or EnterpriseGraph this collection belongs to is disjoint
(Enterprise Edition only). This is an internal property. _(cluster only)_

@RESTSTRUCT{smartGraphAttribute,collection_info,string,optional,}
The attribute that is used for sharding: vertices with the same value of
this attribute are placed in the same shard. All vertices are required to
have this attribute set and it has to be a string. Edges derive the
attribute from their connected vertices (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{smartJoinAttribute,collection_info,string,optional,}
Determines an attribute of the collection that must contain the shard key value
of the referred-to SmartJoin collection (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{name,collection_info,string,optional,}
The name of this collection.

@RESTSTRUCT{id,collection_info,string,optional,}
A unique identifier of the collection (deprecated).

@RESTSTRUCT{type,collection_info,integer,optional,}
The type of the collection:
  - `0`: "unknown"
  - `2`: regular document collection
  - `3`: edge collection

@RESTSTRUCT{isSystem,collection_info,boolean,optional,}
Whether the collection is a system collection. Collection names that starts with
an underscore are usually system collections.

@RESTSTRUCT{syncByRevision,collection_info,boolean,required,}
Whether the newer revision-based replication protocol is
enabled for this collection. This is an internal property.

@RESTSTRUCT{globallyUniqueId,collection_info,string,optional,}
A unique identifier of the collection. This is an internal property.
