
@startDocuBlock REST_DOCUMENT_REPLACE
@brief replaces a document

@RESTHEADER{PUT /_api/document/{document-handle},Replace document}

@RESTALLBODYPARAM{document,json,required}
A JSON representation of a single document.

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
This URL parameter must be a document handle.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until document has been synced to disk.

@RESTQUERYPARAM{ignoreRevs,boolean,optional}
By default, or if this is set to *true*, the *_rev* attributes in 
the given document is ignored. If this is set to *false*, then
the *_rev* attribute given in the body document is taken as a
precondition. The document is only replaced if the current revision
is the one specified.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed 
document under the attribute *old* in the result.

@RESTQUERYPARAM{returnNew,boolean,optional}
Return additionally the complete new document under the attribute *new*
in the result.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally replace a document based on a target revision id by
using the *if-match* HTTP header.

@RESTDESCRIPTION
Replaces the document with handle <document-handle> with the one in
the body, provided there is such a document and no precondition is
violated.

If the *If-Match* header is specified and the revision of the
document in the database is unequal to the given revision, the
precondition is violated.

If *If-Match* is not given and *ignoreRevs* is *false* and there
is a *_rev* attribute in the body and its value does not match
the revision of the document in the database, the precondition is
violated.

If a precondition is violated, an *HTTP 412* is returned.

If the document exists and can be updated, then an *HTTP 201* or
an *HTTP 202* is returned (depending on *waitForSync*, see below),
the *ETag* header field contains the new revision of the document
and the *Location* header contains a complete URL under which the
document can be queried.

Optionally, the query parameter *waitForSync* can be used to force
synchronization of the document replacement operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* query parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

The body of the response contains a JSON object with the information
about the handle and the revision. The attribute *_id* contains the
known *document-handle* of the updated document, *_key* contains the
key which uniquely identifies a document in a given collection, and
the attribute *_rev* contains the new document revision.

If the query parameter *returnOld* is *true*, then
the complete previous revision of the document
is returned under the *old* attribute in the result.

If the query parameter *returnNew* is *true*, then
the complete new document is returned under
the *new* attribute in the result.

If the document does not exist, then a *HTTP 404* is returned and the
body of the response contains an error document.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the document was replaced successfully and
*waitForSync* was *true*.

@RESTRETURNCODE{202}
is returned if the document was replaced successfully and
*waitForSync* was *false*.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of a document. The response body contains
an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection or the document was not found.

@RESTRETURNCODE{412}
is returned if the precondition was violated. The response will
also contain the found documents' current revisions in the *_rev*
attributes. Additionally, the attributes *_id* and *_key* will be
returned.

@EXAMPLES

Using a document handle

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('PUT', url, '{"Hello": "you"}');

    assert(response.code === 202);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown document handle

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentUnknownHandle}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    db.products.remove(document._id);
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('PUT', url, "{}");

    assert(response.code === 404);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Produce a revision conflict

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentIfMatchOther}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var document2 = db.products.save({"hello2":"world"});
    var url = "/_api/document/" + document._id;
    var headers = {"If-Match":  "\"" + document2._rev + "\""};

    var response = logCurlRequest('PUT', url, '{"other":"content"}', headers);

    assert(response.code === 412);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

