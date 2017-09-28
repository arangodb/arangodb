
@startDocuBlock JSF_put_api_collection_load_indexes_into_memory
@brief Load Indexes into Memory

@RESTHEADER{PUT /_api/collection/{collection-name}/loadIndexesIntoMemory, Load Indexes into Memory}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}

@RESTDESCRIPTION
This route tries to cache all index entries
of this collection into the main memory.
Therefore it iterates over all indexes of the collection
and stores the indexed values, not the entire document data,
in memory.
All lookups that could be found in the cache are much faster
than lookups not stored in the cache so you get a nice performance boost.
It is also guaranteed that the cache is consistent with the stored data.

For the time being this function is only useful on RocksDB storage engine,
as in MMFiles engine all indexes are in memory anyways.

On RocksDB this function honors all memory limits, if the indexes you want
to load are smaller than your memory limit this function guarantees that most
index values are cached.
If the index is larger than your memory limit this function will fill up values
up to this limit and for the time being there is no way to control which indexes
of the collection should have priority over others.

On sucess this function returns an object with attribute `result` set to `true`

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the indexes have all been loaded

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

