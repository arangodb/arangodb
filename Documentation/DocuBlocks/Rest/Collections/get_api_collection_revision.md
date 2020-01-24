
@startDocuBlock get_api_collection_revision
@brief Retrieve the collections revision id

@RESTHEADER{GET /_api/collection/{collection-name}/revision, Return collection revision id, handleCommandGet:collectionRevision}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
In addition to the above, the result will also contain the
collection's revision id. The revision id is a server-generated
string that clients can use to check whether data in a collection
has changed since the last revision check.

- *revision*: The collection revision id as a string.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Retrieving the revision of a collection

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionRevision}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: false });
    var url = "/_api/collection/"+ coll.name() + "/revision";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
