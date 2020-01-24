
@startDocuBlock delete_api_collection
@brief drops a collection

@RESTHEADER{DELETE /_api/collection/{collection-name}, Drops a collection, handleCommandDelete:collection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection to drop.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{isSystem,boolean,optional}
Whether or not the collection to drop is a system collection. This parameter
must be set to *true* in order to drop a system collection.

@RESTDESCRIPTION
Drops the collection identified by *collection-name*.

If the collection was successfully dropped, an object is returned with
the following attributes:

- *error*: *false*

- *id*: The identifier of the dropped collection.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestCollectionDeleteCollectionIdentifier}
    var cn = "products1";
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll._id;

    var response = logCurlRequest('DELETE', url);
    db[cn] = undefined;

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestCollectionDeleteCollectionName}
    var cn = "products1";
    db._drop(cn);
    db._create(cn);
    var url = "/_api/collection/products1";

    var response = logCurlRequest('DELETE', url);
    db[cn] = undefined;

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Dropping a system collection

@EXAMPLE_ARANGOSH_RUN{RestCollectionDeleteCollectionSystem}
    var cn = "_example";
    db._drop(cn, { isSystem: true });
    db._create(cn, { isSystem: true });
    var url = "/_api/collection/_example?isSystem=true";

    var response = logCurlRequest('DELETE', url);
    db[cn] = undefined;

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
