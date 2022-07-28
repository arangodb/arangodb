@RESTSTRUCT{waitForSync,collection_info,boolean,required,}
If `true` then creating, changing or removing
documents will wait until the data has been synchronized to disk.

@RESTSTRUCT{schema,collection_info,object,optional,}
The collection level schema for documents.

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
A object which contains key generation options

@RESTSTRUCT{type,key_generator_type,string,required,}
specifies the type of the key generator. The currently
available generators are `traditional`, `autoincrement`, `uuid`
and `padded`.

@RESTSTRUCT{cacheEnabled,collection_info,boolean,required,}
Whether the in-memory hash cache for documents is enabled for this
collection.

@RESTSTRUCT{allowUserKeys,key_generator_type,boolean,required,}
if set to `true`, then it is allowed to supply
own key values in the `_key` attribute of a document. If set to
`false`, then the key generator is solely responsible for
generating keys and supplying own key values in the `_key` attribute
of documents is considered an error.

@RESTSTRUCT{lastValue,key_generator_type,integer,required,}

@RESTSTRUCT{numberOfShards,collection_info,integer,optional,}
The number of shards of the collection. _(cluster only)_

@RESTSTRUCT{shardKeys,collection_info,array,optional,string}
contains the names of document attributes that are used to
determine the target shard for documents. _(cluster only)_

@RESTSTRUCT{replicationFactor,collection_info,integer,optional,}
contains how many copies of each shard are kept on different DB-Servers.
It is an integer number in the range of 1-10 or the string `"satellite"`
for a SatelliteCollection (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{writeConcern,collection_info,integer,optional,}
determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
`writeConcern` cannot be larger than `replicationFactor`. _(cluster only)_

@RESTSTRUCT{shardingStrategy,collection_info,string,optional,}
the sharding strategy selected for the collection.
One of 'hash' or 'enterprise-hash-smart-edge'. _(cluster only)_

@RESTSTRUCT{isSmart,collection_info,boolean,optional,}
Whether the collection is used in a SmartGraph (Enterprise Edition only).
_(cluster only)_

@RESTSTRUCT{smartGraphAttribute,collection_info,string,optional,}
Attribute that is used in SmartGraphs (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{smartJoinAttribute,collection_info,string,optional,}
Determines an attribute of the collection that must contain the shard key value
of the referred-to SmartJoin collection (Enterprise Edition only). _(cluster only)_

@RESTSTRUCT{isSystem,collection_info,boolean,optional,}
true if this is a system collection; usually `name` will start with an underscore.

@RESTSTRUCT{name,collection_info,string,optional,}
literal name of this collection

@RESTSTRUCT{id,collection_info,string,optional,}
unique identifier of the collection; *deprecated*

@RESTSTRUCT{type,collection_info,integer,optional,}
The type of the collection:
  - `0`: "unknown"
  - `2`: regular document collection
  - `3`: edge collection

@RESTSTRUCT{globallyUniqueId,collection_info,string,optional,}
Unique identifier of the collection
