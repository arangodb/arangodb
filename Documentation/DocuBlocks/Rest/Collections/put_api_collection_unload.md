
@startDocuBlock put_api_collection_unload
@brief unloads a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/unload, Unload collection, handleCommandPut:collectionUnload}

@HINTS
{% hint 'warning' %}
The unload function is deprecated from version 3.8.0 onwards and is a no-op 
from version 3.9.0 onwards. It should no longer be used, as it may be removed
in a future version of ArangoDB.
{% endhint %}

{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}

@RESTDESCRIPTION
Since ArangoDB version 3.9.0 this API does nothing. Previously it used to
unload a collection from memory, while preserving all documents.
When calling the API an object with the following attributes is
returned for compatibility reasons:

- *id*: The identifier of the collection.

- *name*: The name of the collection.

- *status*: The status of the collection as number.

- *type*: The collection type. Valid types are:
  - 2: document collection
  - 3: edges collection

- *isSystem*: If *true* then the collection is a system collection.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierUnload}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll.name() + "/unload";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
