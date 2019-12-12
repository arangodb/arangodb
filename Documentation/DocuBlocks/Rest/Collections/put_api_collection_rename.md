
@startDocuBlock put_api_collection_rename
@brief renames a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/rename, Rename collection, handleCommandPut:renameCollection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection to rename.

@RESTDESCRIPTION
Renames a collection. Expects an object with the attribute(s)

- *name*: The new name.

It returns an object with the attributes

- *id*: The identifier of the collection.

- *name*: The new name of the collection.

- *status*: The status of the collection as number.

- *type*: The collection type. Valid types are:
  - 2: document collection
  - 3: edges collection

- *isSystem*: If *true* then the collection is a system collection.

If renaming the collection succeeds, then the collection is also renamed in
all graph definitions inside the `_graphs` collection in the current database.

**Note**: this method is not available in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.
@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierRename}
    var cn = "products1";
    var cnn = "newname";
    db._drop(cn);
    db._drop(cnn);
    var coll = db._create(cn);
    var url = "/_api/collection/" + coll.name() + "/rename";

    var response = logCurlRequest('PUT', url, { name: cnn });

    assert(response.code === 200);
    db._flushCache();
    db._drop(cnn);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
