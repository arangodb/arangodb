
@startDocuBlock get_api_collection_shards
@brief Return the shard ids of a collection

@RESTHEADER{GET /_api/collection/{collection-name}/shards, Return the shard ids of a collection, shards:Collection}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{details,boolean,optional}
If set to true, the return value will also contain the responsible servers for the collections' shards.

@RESTDESCRIPTION
By default returns a JSON array with the shard IDs of the collection.

If the `details` parameter is set to `true`, it will return a JSON object with the
shard IDs as object attribute keys, and the responsible servers for each shard mapped to them.
In the detailed response, the leader shards will be first in the arrays.

**Note** : This method is only available in a cluster Coordinator.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns the collection's shards.

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then an *HTTP 404*
is returned.

@RESTRETURNCODE{501}
*HTTP 501* is returned if the method is called on a single server.

@EXAMPLES

Retrieves the list of shards:

@EXAMPLE_ARANGOSH_RUN{RestGetShards_cluster}
    var cn = "testCollection";
    db._drop(cn);
    db._create(cn, { numberOfShards: 3 });

    var response = logCurlRequest('GET', "/_api/collection/" + cn + "/shards");

    assert(response.code === 200);
    logRawResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Retrieves the list of shards with the responsible servers:

@EXAMPLE_ARANGOSH_RUN{RestGetShardsWithDetails_cluster}
    var cn = "testCollection";
    db._drop(cn);
    db._create(cn, { numberOfShards: 3 });

    var response = logCurlRequest('GET', "/_api/collection/" + cn + "/shards?details=true");

    assert(response.code === 200);
    logRawResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
