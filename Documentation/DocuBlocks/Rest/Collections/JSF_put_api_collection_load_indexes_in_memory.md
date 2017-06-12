
@startDocuBlock JSF_put_api_collection_loadindexesinmemory
@brief Load Indexes into Memory

@RESTHEADER{PUT /_api/collection/{collection-name}/loadIndexesInMemory, Load Indexes into Memory}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}

@RESTDESCRIPTION
This route tries to cache all index entries
of this collection into the main memory.
Therefore it iterates over all indexes of the collection
and stores the indexed values, not the entire document data,
in Memory.
All lookups that could be found in the cache are much faster
than lookups not stored in the cache so you get a nice performance boost.
It is also guaranteed that the cache is consistent with the stored data.

For the time beeing this function is only useful on RocksDB storage engine,
as in MMFiles engine all indexes are in memory anyways.

On RocksDB this function honors all memory limits, if the indexes you want
to load are smaller than your memory limit this function guarantees that most
index values are cached.
If the index is larger than your memory limit this function will fill up values
up to this limit and for the time beeing there is no way to control which indexes
of the collection should have priority over others.

On sucess this function returns `true`

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the indexes have all been loaded

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierLoadIndexesInMemory}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    var url = "/_api/collection/"+ coll.name() + "/loadIndexesInMemory";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

