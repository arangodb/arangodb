
@startDocuBlock REST_DOCUMENT_UPDATE
@brief updates a document

@RESTHEADER{PATCH /_api/document/{document-handle},Update document}

@RESTALLBODYPARAM{document,json,required}
A JSON representation of a document update as an object.

@RESTURLPARAMETERS

@RESTURLPARAM{document-handle,string,required}
This URL parameter must be a document handle.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{keepNull,boolean,optional}
If the intention is to delete existing attributes with the patch
command, the URL query parameter *keepNull* can be used with a value
of *false*. This will modify the behavior of the patch command to
remove any attributes from the existing document that are contained
in the patch document with an attribute value of *null*.

@RESTQUERYPARAM{mergeObjects,boolean,optional}
Controls whether objects (not arrays) will be merged if present in
both the existing and the patch document. If set to *false*, the
value in the patch document will overwrite the existing document's
value. If set to *true*, objects will be merged. The default is
*true*.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until document has been synced to disk.

@RESTQUERYPARAM{ignoreRevs,boolean,optional}
By default, or if this is set to *true*, the *_rev* attributes in 
the given document is ignored. If this is set to *false*, then
the *_rev* attribute given in the body document is taken as a
precondition. The document is only updated if the current revision
is the one specified.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed 
document under the attribute *old* in the result.

@RESTQUERYPARAM{returnNew,boolean,optional}
Return additionally the complete new document under the attribute *new*
in the result.

@RESTQUERYPARAM{silent,boolean,optional}
If set to *true*, an empty object will be returned as response. No meta-data 
will be returned for the updated document. This option can be used to
save some network traffic.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
You can conditionally update a document based on a target revision id by
using the *if-match* HTTP header.

@RESTDESCRIPTION
Partially updates the document identified by *document-handle*.
The body of the request must contain a JSON document with the
attributes to patch (the patch document). All attributes from the
patch document will be added to the existing document if they do not
yet exist, and overwritten in the existing document if they do exist
there.

Setting an attribute value to *null* in the patch document will cause a
value of *null* to be saved for the attribute by default.

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
the *Etag* header field contains the new revision of the document
(in double quotes) and the *Location* header contains a complete URL
under which the document can be queried.

Optionally, the query parameter *waitForSync* can be used to force
synchronization of the updated document operation to disk even in case
that the *waitForSync* flag had been disabled for the entire collection.
Thus, the *waitForSync* query parameter can be used to force synchronization
of just specific operations. To use this, set the *waitForSync* parameter
to *true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

If *silent* is not set to *true*, the body of the response contains a JSON 
object with the information about the handle and the revision. The attribute 
*_id* contains the known *document-handle* of the updated document, *_key* 
contains the key which uniquely identifies a document in a given collection, 
and the attribute *_rev* contains the new document revision.

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
is returned if the document was updated successfully and
*waitForSync* was *true*.

@RESTRETURNCODE{202}
is returned if the document was updated successfully and
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

Patches an existing document with new content.

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPatchDocument}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"one":"world"});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest("PATCH", url, { "hello": "world" });

    assert(response.code === 202);

    logJsonResponse(response);
    var response2 = logCurlRequest("PATCH", url, { "numbers": { "one": 1, "two": 2, "three": 3, "empty": null } });
    assert(response2.code === 202);
    logJsonResponse(response2);
    var response3 = logCurlRequest("GET", url);
    assert(response3.code === 200);
    logJsonResponse(response3);
    var response4 = logCurlRequest("PATCH", url + "?keepNull=false", { "hello": null, "numbers": { "four": 4 } });
    assert(response4.code === 202);
    logJsonResponse(response4);
    var response5 = logCurlRequest("GET", url);
    assert(response5.code === 200);
    logJsonResponse(response5);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Merging attributes of an object using `mergeObjects`:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPatchDocumentMerge}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"inhabitants":{"china":1366980000,"india":1263590000,"usa":319220000}});
    var url = "/_api/document/" + document._id;

    var response = logCurlRequest("GET", url);
    assert(response.code === 200);
    logJsonResponse(response);

    var response = logCurlRequest("PATCH", url + "?mergeObjects=true", { "inhabitants": {"indonesia":252164800,"brazil":203553000 }});
    assert(response.code === 202);

    var response2 = logCurlRequest("GET", url);
    assert(response2.code === 200);
    logJsonResponse(response2);

    var response3 = logCurlRequest("PATCH", url + "?mergeObjects=false", { "inhabitants": { "pakistan":188346000 }});
    assert(response3.code === 202);
    logJsonResponse(response3);

    var response4 = logCurlRequest("GET", url);
    assert(response4.code === 200);
    logJsonResponse(response4);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

