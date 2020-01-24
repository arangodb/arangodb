
@startDocuBlock get_api_collection_getResponsibleShard
@brief Return the responsible shard for a document

@RESTHEADER{PUT /_api/collection/{collection-name}/responsibleShard, Return responsible shard for a document, getResponsibleShard:Collection}

@RESTALLBODYPARAM{document,json,required}
The body must consist of a JSON object with at least the shard key
attributes set to some values.

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Returns the ID of the shard that is responsible for the given document
(if the document exists) or that would be responsible if such document
existed.

The request must body must contain a JSON document with at least the
collection's shard key attributes set to some values.

The response is a JSON object with a *shardId* attribute, which will
contain the ID of the responsible shard.

**Note** : This method is only available in a cluster Coordinator.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns the ID of the responsible shard.

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.
Additionally, if not all of the collection's shard key
attributes are present in the input document, then a
*HTTP 400* is returned as well.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then an *HTTP 404*
is returned.

@RESTRETURNCODE{501}
*HTTP 501* is returned if the method is called on a single server.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestGetResponsibleShardExample_cluster}
    var cn = "testCollection";
    db._drop(cn);
    db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

    var body = JSON.stringify({ _key: "testkey", value: 23 });
    var response = logCurlRequestRaw('PUT', "/_api/collection/" + cn + "/responsibleShard", body);

    assert(response.code === 200);
    assert(JSON.parse(response.body).hasOwnProperty("shardId"));

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
