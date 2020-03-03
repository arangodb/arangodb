
@startDocuBlock get_api_collection_count
@brief Counts the documents in a collection

@RESTHEADER{GET /_api/collection/{collection-name}/count, Return number of documents in a collection, handleCommandGet:getCollectionCount}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
In addition to the above, the result also contains the number of documents.
**Note** that this will always load the collection into memory.

- *count*: The number of documents inside the collection.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Requesting the number of documents:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionCount}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    for(var i=0;i<100;i++) {
       coll.save({"count" :  i });
    }
    var url = "/_api/collection/"+ coll.name() + "/count";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
