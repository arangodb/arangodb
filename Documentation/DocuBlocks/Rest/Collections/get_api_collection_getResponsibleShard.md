
@startDocuBlock get_api_collection_getResponsibleShard
@brief Return the responsible shard for a document

@RESTHEADER{PUT /_api/collection/{collection-name}/responsibleShard, Return responsible shard for a document}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Returns the ID of the shard that is responsible for the given document
(if the document exists) or that would be responsible if such document
existed.

The response is a JSON object with a *shardId* attribute, which will 
contain the ID of the responsible shard.

**Note** : This method is only available in a cluster coordinator.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns the ID of the responsible shard:

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then an *HTTP 404*
is returned.

@endDocuBlock
