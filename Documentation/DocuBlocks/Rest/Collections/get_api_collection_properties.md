
@startDocuBlock get_api_collection_properties
@brief reads the properties of the specified collection

@RESTHEADER{GET /_api/collection/{collection-name}/properties, Read properties of a collection, handleCommandGet:collectionProperties}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@RESTRETURNCODE{200}

@RESTREPLYBODY{,object,required,collection_info}

@RESTDESCRIPTION

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionIdentifier}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll._id + "/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionName}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var url = "/_api/collection/products/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
