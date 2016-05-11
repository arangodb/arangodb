
@startDocuBlock API_EDGE_READ_ALL
@brief reads all edges from collection

@RESTHEADER{GET /_api/edge, Read all edges from collection}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The name of the collection.

@RESTDESCRIPTION
Returns an array of all URIs for all edges from the collection identified
by *collection*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
All went good.

@RESTRETURNCODE{404}
The collection does not exist.
@endDocuBlock

