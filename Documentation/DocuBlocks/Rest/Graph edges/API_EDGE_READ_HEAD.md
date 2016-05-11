
@startDocuBlock API_EDGE_READ_HEAD
@brief reads a single edge head

@RESTHEADER{HEAD /_api/edge/{document-handle}, Read edge header}

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
The handle of the edge document.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{rev,string,optional}
You can conditionally fetch an edge document based on a target revision id by
using the *rev* query parameter.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-None-Match,string,optional}
If the "If-None-Match" header is given, then it must contain exactly one
etag. If the current document revision is different to the specified etag,
an *HTTP 200* response is returned. If the current document revision is
identical to the specified etag, then an *HTTP 304* is returned.

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally fetch an edge document based on a target revision id by
using the *if-match* HTTP header.

@RESTDESCRIPTION
Like *GET*, but only returns the header fields and not the body. You
can use this call to get the current revision of an edge document or check if
it was deleted.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the edge document was found

@RESTRETURNCODE{304}
is returned if the "If-None-Match" header is given and the edge document has
same version

@RESTRETURNCODE{404}
is returned if the edge document or collection was not found

@RESTRETURNCODE{412}
is returned if a "If-Match" header or *rev* is given and the found
document has a different version. The response will also contain the found
document's current revision in the *etag* header.
@endDocuBlock

