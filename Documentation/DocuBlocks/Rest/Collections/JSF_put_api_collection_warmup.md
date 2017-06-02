
@startDocuBlock JSF_put_api_collection_warmup
@brief Warmup index caches 

@RESTHEADER{PUT /_api/collection/{collection-name}/warmup, Warmup Collection}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}

@RESTDESCRIPTION
Warms up indexes caches of a collection.
Iterates over all indexes of a collection and
reads data into index caches to speed up retrival.
Index types that do not support the operation are
silently ignored.


- *id*: The identifier of the collection.

- *name*: The name of the collection.

- *status*: The status of the collection as number.

- *type*: The collection type. Valid types are:
  - 2: document collection
  - 3: edges collection

- *isSystem*: If *true* then the collection is a system collection.

supported index types: Edge Index (RocksDB)

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierWarmup}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll.name() + "/warmup";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

