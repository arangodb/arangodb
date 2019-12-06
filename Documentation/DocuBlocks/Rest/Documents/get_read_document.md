
@startDocuBlock get_read_document
@brief reads a single document

@RESTHEADER{GET /_api/document/{collection}/{key},Read document,readDocument}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the *collection* from which the document is to be read.

@RESTURLPARAM{key,string,required}
The document key.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-None-Match,string,optional}
If the "If-None-Match" header is given, then it must contain exactly one
Etag. The document is returned, if it has a different revision than the
given Etag. Otherwise an *HTTP 304* is returned.

@RESTHEADERPARAM{If-Match,string,optional}
If the "If-Match" header is given, then it must contain exactly one
Etag. The document is returned, if it has the same revision as the
given Etag. Otherwise a *HTTP 412* is returned.

@RESTDESCRIPTION
Returns the document identified by *document-id*. The returned
document contains three special attributes: *_id* containing the document
identifier, *_key* containing key which uniquely identifies a document
in a given collection and *_rev* containing the revision.

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
document's current revision in the *_rev* attribute. Additionally, the
attributes *_id* and *_key* will be returned.

@EXAMPLES

Use a document identifier:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Use a document identifier and an Etag:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentIfNoneMatch}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var url = "/_api/document/" + document._id;
    var headers = {"If-None-Match": "\"" + document._rev + "\""};

    var response = logCurlRequest('GET', url, "", headers);

    assert(response.code === 304);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown document identifier:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentUnknownHandle}
    var url = "/_api/document/products/unknown-identifier";

    var response = logCurlRequest('GET', url);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
