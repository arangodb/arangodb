
@startDocuBlock put_api_collection_recalculate_count
@brief recalculates the document count of a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/recalculateCount, Recalculate count of a collection}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Recalculates the document count of a collection, if it ever becomes inconsistent.

It returns an object with the attributes

- *result*: will be *true* if rotation succeeded

**Note**: this method is specific for the RocksDB storage engine

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the collection currently has no journal, *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.
@endDocuBlock

