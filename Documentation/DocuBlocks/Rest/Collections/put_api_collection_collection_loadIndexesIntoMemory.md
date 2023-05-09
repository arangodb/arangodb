
@startDocuBlock put_api_collection_collection_loadIndexesIntoMemory
@brief Load Indexes into Memory

@RESTHEADER{PUT /_api/collection/{collection-name}/loadIndexesIntoMemory, Load Indexes into Memory, loadCollectionIndexes}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
You can call this endpoint to try to cache this collection's index entries in
the main memory. Index lookups served from the memory cache can be much faster
than lookups not stored in the cache, resulting in a performance boost.

The endpoint iterates over suitable indexes of the collection and stores the
indexed values (not the entire document data) in memory. This is implemented for
edge indexes only.

The endpoint returns as soon as the index warmup has been scheduled. The index
warmup may still be ongoing in the background, even after the return value has
already been sent. As all suitable indexes are scanned, it may cause significant
I/O activity and background load.

This feature honors memory limits. If the indexes you want to load are smaller
than your memory limit, this feature guarantees that most index values are
cached. If the index is greater than your memory limit, this feature fills
up values up to this limit. You cannot control which indexes of the collection
should have priority over others.

It is guaranteed that the in-memory cache data is consistent with the stored
index data at all times.

On success, this endpoint returns an object with attribute `result` set to `true`.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index loading has been scheduled for all suitable indexes.

@RESTRETURNCODE{400}
If the `collection-name` is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the `collection-name` is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierLoadIndexesIntoMemory}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    var url = "/_api/collection/"+ coll.name() + "/loadIndexesIntoMemory";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
