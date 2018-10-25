
@startDocuBlock put_api_collection_truncate
@brief truncates a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/truncate, Truncate collection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Removes all documents from the collection, but leaves the indexes intact.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierTruncate}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll.name() + "/truncate";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

