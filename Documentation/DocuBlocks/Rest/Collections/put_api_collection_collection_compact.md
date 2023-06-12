
@startDocuBlock put_api_collection_collection_compact

@RESTHEADER{PUT /_api/collection/{collection-name}/compact, Compact a collection, compactCollection}

@RESTDESCRIPTION
Compacts the data of a collection in order to reclaim disk space.
The operation will compact the document and index data by rewriting the
underlying .sst files and only keeping the relevant entries.

Under normal circumstances, running a compact operation is not necessary, as
the collection data will eventually get compacted anyway. However, in some
situations, e.g. after running lots of update/replace or remove operations,
the disk data for a collection may contain a lot of outdated data for which the
space shall be reclaimed. In this case the compaction operation can be used.

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
Name of the collection to compact

@RESTRETURNCODES

@RESTRETURNCODE{200}
Compaction started successfully

@RESTRETURNCODE{401}
if the request was not authenticated as a user with sufficient rights

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestApiCollectionCompact}
    var cn = "testCollection";
    db._drop(cn);
    db._create(cn);

    var response = logCurlRequest('PUT', '/_api/collection/' + cn + '/compact', '');

    assert(response.code === 200);

    logJsonResponse(response);

    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
