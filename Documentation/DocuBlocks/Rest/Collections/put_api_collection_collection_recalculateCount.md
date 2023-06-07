
@startDocuBlock put_api_collection_collection_recalculateCount

@RESTHEADER{PUT /_api/collection/{collection-name}/recalculateCount, Recalculate the document count of a collection, recalculateCollectionCount}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Recalculates the document count of a collection, if it ever becomes inconsistent.

It returns an object with the attributes

- `result`: will be `true` if recalculating the document count succeeded.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the document count was recalculated successfully, *HTTP 200* is returned.

@RESTRETURNCODE{404}
If the `collection-name` is unknown, then a *HTTP 404* is returned.

@endDocuBlock
