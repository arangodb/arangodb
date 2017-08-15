
@startDocuBlock JSF_post_api_collection
@brief creates a collection

@RESTHEADER{POST /_api/collection, Create collection}

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

@RESTBODYPARAM{keyOptions,object,optional,JSF_post_api_collection_opts}
additional options for key generation. If specified, then *keyOptions*
should be a JSON array containing the following attributes:

@RESTSTRUCT{type,JSF_post_api_collection_opts,string,required,string}
specifies the type of the key generator. The currently available generators are
*traditional* and *autoincrement*.

@RESTSTRUCT{allowUserKeys,JSF_post_api_collection_opts,boolean,required,}
if set to *true*, then it is allowed to supply own key values in the
*_key* attribute of a document. If set to *false*, then the key generator
will solely be responsible for generating keys and supplying own key values
in the *_key* attribute of documents is considered an error.

@RESTSTRUCT{increment,JSF_post_api_collection_opts,integer,required,int64}
increment value for *autoincrement* key generator. Not used for other key
generator types.

@RESTSTRUCT{offset,JSF_post_api_collection_opts,integer,required,int64}
Initial offset value for *autoincrement* key generator.
Not used for other key generator types.

@RESTBODYPARAM{type,integer,optional,int64}
(The default is *2*): the type of the collection to create.
The following values for *type* are valid:

- *2*: document collection
- *3*: edges collection

@RESTBODYPARAM{indexBuckets,integer,optional,int64}
The number of buckets into which indexes using a hash
table are split. The default is 16 and this number has to be a
power of 2 and less than or equal to 1024. 

For very large collections one should increase this to avoid long pauses 
when the hash table has to be initially built or resized, since buckets 
are resized individually and can be initially built in parallel. For 
example, 64 might be a sensible value for a collection with 100
000 000 documents. Currently, only the edge index respects this
value, but other index types might follow in future ArangoDB versions. 
Changes (see below) are applied when the collection is loaded the next 
time.
This option is meaningful for the MMFiles storage engine only.

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
of each shard are kept on different DBServers. The value 1 means that only one 
copy (no synchronous replication) is kept. A value of k means that k-1 replicas 
are kept. Any two copies reside on different DBServers. Replication between them is 
synchronous, that is, every write operation to the "leader" copy will be replicated 
to all "follower" replicas, before the write operation is reported successful.

If a server fails, this is detected automatically and one of the servers holding 
copies take over, usually without an error being reported.

@RESTQUERYPARAMETERS 
@RESTQUERYPARAM{waitForSyncReplication,integer,optional,int64}
Default is *1* which means the server will only report success back to the client
if all replicas have created the collection. Set to *0* if you want faster
server responses and don't care about full replication.

@RESTDESCRIPTION
Creates a new collection with a given name. The request must contain an
object with the following attributes.


@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.


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

