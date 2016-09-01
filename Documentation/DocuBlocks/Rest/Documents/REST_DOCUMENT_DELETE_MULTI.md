@startDocuBlock REST_DOCUMENT_DELETE_MULTI
@brief removes multiple document

@RESTHEADER{DELETE /_api/document/{collection},Removes multiple documents}

@RESTALLBODYPARAM{array,json,required}
A JSON array of strings or documents.

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Collection from which documents are removed.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until deletion operation has been synced to disk.

@RESTQUERYPARAM{returnOld,boolean,optional}
Return additionally the complete previous revision of the changed 
document under the attribute *old* in the result.

@RESTQUERYPARAM{ignoreRevs,boolean,optional}
If set to *true*, ignore any *_rev* attribute in the selectors. No
revision check is performed.

@RESTDESCRIPTION
The body of the request is an array consisting of selectors for
documents. A selector can either be a string with a key or a string
with a document handle or an object with a *_key* attribute. This
API call removes all specified documents from *collection*. If the
selector is an object and has a *_rev* attribute, it is a
precondition that the actual revision of the removed document in the
collection is the specified one.

The body of the response is an array of the same length as the input
array. For each input selector, the output contains a JSON object
with the information about the outcome of the operation. If no error
occurred, an object is built in which the attribute *_id* contains
the known *document-handle* of the removed document, *_key* contains
the key which uniquely identifies a document in a given collection,
and the attribute *_rev* contains the document revision. In case of
an error, an object with the attribute *error* set to *true* and
*errorCode* set to the error code is built.

If the *waitForSync* parameter is not specified or set to *false*,
then the collection's default *waitForSync* behavior is applied.
The *waitForSync* query parameter cannot be used to disable
synchronization for collections that have a default *waitForSync*
value of *true*.

If the query parameter *returnOld* is *true*, then
the complete previous revision of the document
is returned under the *old* attribute in the result.

Note that if any precondition is violated or an error occurred with
some of the documents, the return code is still 200 or 202, but
the additional HTTP header *X-Arango-Error-Codes* is set, which
contains a map of the error codes that occurred together with their
multiplicities, as in: *1200:17,1205:10* which means that in 17
cases the error 1200 "revision conflict" and in 10 cases the error
1205 "illegal document handle" has happened.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if *waitForSync* was *true*.

@RESTRETURNCODE{202}
is returned if *waitForSync* was *false*.

@RESTRETURNCODE{404}
is returned if the collection was not found.
The response body contains an error document in this case.

@EXAMPLES

Using document handle:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentMulti}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var document = db.products.save({"hello":"world"});

    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown document handle:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentUnknownHandleMulti}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var document = db.products.save({"hello":"world"});
    db.products.remove(document._id);

    var url = "/_api/document/" + document._id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 404);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Revision conflict:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentIfMatchOtherMulti}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var document = db.products.save({"hello":"world"});
    var document2 = db.products.save({"hello2":"world"});
    var url = "/_api/document/" + document._id;
    var headers = {"If-Match":  "\"" + document2._rev + "\""};

    var response = logCurlRequest('DELETE', url, "", headers);

    assert(response.code === 412);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
