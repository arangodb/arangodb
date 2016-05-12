
@startDocuBlock API_EDGE_READ
@brief reads a single edge

@RESTHEADER{GET /_api/document/{document-handle}, Read edge}

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
The handle of the edge document.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-None-Match,string,optional}
If the "If-None-Match" header is given, then it must contain exactly one
etag. The edge is returned if it has a different revision than the
given etag. Otherwise an *HTTP 304* is returned.

@RESTHEADERPARAM{If-Match,string,optional}
If the "If-Match" header is given, then it must contain exactly one
etag. The edge is returned if it has the same revision ad the
given etag. Otherwise a *HTTP 412* is returned. As an alternative
you can supply the etag in an attribute *rev* in the URL.

@RESTDESCRIPTION
Returns the edge identified by *document-handle*. The returned
edge contains a few special attributes:

- *_id* contains the document handle

- *_rev* contains the revision

- *_from* and *to* contain the document handles of the connected
  vertex documents

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the edge was found

@RESTRETURNCODE{304}
is returned if the "If-None-Match" header is given and the edge has
the same version

@RESTRETURNCODE{404}
is returned if the edge or collection was not found

@RESTRETURNCODE{412}
is returned if a "If-Match" header or *rev* is given and the found
document has a different version. The response will also contain the found
document's current revision in the *_rev* attribute. Additionally, the
attributes *_id* and *_key* will be returned.
@endDocuBlock

