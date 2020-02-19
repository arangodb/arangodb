
@startDocuBlock head_read_document_header
@brief reads a single document head

@RESTHEADER{HEAD /_api/document/{collection}/{key},Read document header,checkDocument}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the *collection* from which the document is to be read.

@RESTURLPARAM{key,string,required}
The document key.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-None-Match,string,optional}
If the "If-None-Match" header is given, then it must contain exactly one
Etag. If the current document revision is not equal to the specified Etag,
an *HTTP 200* response is returned. If the current document revision is
identical to the specified Etag, then an *HTTP 304* is returned.

@RESTHEADERPARAM{If-Match,string,optional}
If the "If-Match" header is given, then it must contain exactly one
Etag. The document is returned, if it has the same revision as the
given Etag. Otherwise a *HTTP 412* is returned.

@RESTDESCRIPTION
Like *GET*, but only returns the header fields and not the body. You
can use this call to get the current revision of a document or check if
the document was deleted.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the document was found

@RESTRETURNCODE{304}
is returned if the "If-None-Match" header is given and the document has
the same version

@RESTRETURNCODE{404}
is returned if the document or collection was not found

@RESTRETURNCODE{412}
is returned if an "If-Match" header is given and the found
document has a different version. The response will also contain the found
document's current revision in the *Etag* header.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentHead}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('HEAD', url);

    assert(response.code === 200);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
