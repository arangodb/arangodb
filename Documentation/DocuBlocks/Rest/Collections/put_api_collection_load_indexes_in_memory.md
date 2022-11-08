
@startDocuBlock put_api_collection_load_indexes_into_memory
@brief Load Indexes into Memory

@RESTHEADER{PUT /_api/collection/{collection-name}/loadIndexesIntoMemory, Load Indexes into Memory, handleCommandPut:loadIndexes}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}

@RESTDESCRIPTION
This route tries to cache index entries of this collection in main memory.
Index lookups that can be served from the memory cache can be much faster
than lookups not stored in the cache, so you can get a nice performance boost.

The endpoint iterates over suitable indexes of the collection and stores the 
indexed values, not the entire document data, in memory.
Currently this is implemented only for edge indexes.

The endpoint returns as soon as the index warmup has been scheduled. The index
warmup may still be ongoing in background even after the return value has already
been sent. As all suitable indexes will be scanned, it may cause significant
I/O activity and background load.

This function honors memory limits. If the indexes you want to load are smaller
than your memory limit this function guarantees that most index values are
cached. If the index is larger than your memory limit this function will fill
up values up to this limit and for the time being there is no way to control
which indexes of the collection should have priority over others.

It is guaranteed at all times that the in-memory cache data is consistent with 
the stored index data.

On success this endpoint returns an object with attribute `result` set to `true`

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index loading has been scheduled for all suitable indexes

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

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
