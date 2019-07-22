@RESTSTRUCT{waitForSync,collection_info,boolean,required,}
If *true* then creating, changing or removing
documents will wait until the data has been synchronized to disk.

@RESTSTRUCT{doCompact,collection_info,boolean,required,}
Whether or not the collection will be compacted.
This option is only present for the MMFiles storage engine.

@RESTSTRUCT{journalSize,collection_info,integer,required,}
The maximal size setting for journals / datafiles
in bytes. This option is only present for the MMFiles storage engine.

@RESTSTRUCT{keyOptions,collection_info,object,required,key_generator_type}
A object which contains key generation options

@RESTSTRUCT{type,key_generator_type,string,required,}
specifies the type of the key generator. The currently
available generators are *traditional*, *autoincrement*, *uuid*
and *padded*.

@RESTSTRUCT{allowUserKeys,key_generator_type,boolean,required,}
if set to *true*, then it is allowed to supply
own key values in the *_key* attribute of a document. If set to
*false*, then the key generator is solely responsible for
generating keys and supplying own key values in the *_key* attribute
of documents is considered an error.

@RESTSTRUCT{lastValue,key_generator_type,integer,required,}


@RESTSTRUCT{isVolatile,collection_info,boolean,required,}
If *true* then the collection data will be
kept in memory only and ArangoDB will not write or sync the data
to disk. This option is only present for the MMFiles storage engine.

@RESTSTRUCT{numberOfShards,collection_info,integer,optional,}
The number of shards of the collection; *Cluster specific attribute.*

@RESTSTRUCT{shardKeys,collection_info,array,optional,string}
contains the names of document attributes that are used to
determine the target shard for documents; *Cluster specific attribute.*

@RESTSTRUCT{replicationFactor,collection_info,integer,optional,}
contains how many copies of each shard are kept on different DBServers.; *Cluster specific attribute.*

@RESTSTRUCT{minReplicationFactor,collection_info,integer,optional,}
contains how many minimal copies of each shard are kept on different DBServers.
The shards will refuse to write, if we have less then these many copies in sync.; *Cluster specific attribute.*

@RESTSTRUCT{shardingStrategy,collection_info,string,optional,}
the sharding strategy selected for the collection; *Cluster specific attribute.*
One of 'hash' or 'enterprise-hash-smart-edge'

@RESTSTRUCT{smartGraphAttribute,collection_info,string,optional,}
Attribute that is used in smart graphs, *Cluster specific attribute.*

@RESTSTRUCT{indexBuckets,collection_info,integer,optional,}
the number of index buckets
*Only relevant for the MMFiles storage engine*

@RESTSTRUCT{isSystem,collection_info,boolean,optional,}
true if this is a system collection; usually *name* will start with an underscore.

@RESTSTRUCT{name,collection_info,string,optional,}
literal name of this collection

@RESTSTRUCT{id,collection_info,string,optional,}
unique identifier of the collection; *deprecated*

@RESTSTRUCT{type,collection_info,integer,optional,}
The type of the collection:
  - 0: "unknown"
  - 2: regular document collection
  - 3: edge collection

@RESTSTRUCT{status,collection_info,string,optional,}
corrosponds to **statusString**; *Only relevant for the MMFiles storage engine*
  - 0: "unknown" - may be corrupted
  - 1: (deprecated, maps to "unknown")
  - 2: "unloaded"
  - 3: "loaded"
  - 4: "unloading"
  - 5: "deleted"
  - 6: "loading"



@RESTSTRUCT{statusString,collection_info,string,optional,}
any of: ["unloaded", "loading", "loaded", "unloading", "deleted", "unknown"] *Only relevant for the MMFiles storage engine*

@RESTSTRUCT{globallyUniqueId,collection_info,string,optional,}
Unique identifier of the collection
