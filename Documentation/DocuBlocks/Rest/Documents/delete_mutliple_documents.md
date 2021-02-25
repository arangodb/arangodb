@startDocuBlock delete_remove_document_MULTI
@brief removes multiple document

@RESTHEADER{DELETE /_api/document/{collection},Removes multiple documents,removeDocuments}

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
revision check is performed. If set to *false* then revisions are checked.
The default is *true*.

@RESTDESCRIPTION
The body of the request is an array consisting of selectors for
documents. A selector can either be a string with a key or a string
with a document identifier or an object with a *_key* attribute. This
API call removes all specified documents from *collection*.
If the *ignoreRevs* query parameter is *false* and the
selector is an object and has a *_rev* attribute, it is a
precondition that the actual revision of the removed document in the
collection is the specified one.

The body of the response is an array of the same length as the input
array. For each input selector, the output contains a JSON object
with the information about the outcome of the operation. If no error
occurred, an object is built in which the attribute *_id* contains
the known *document-id* of the removed document, *_key* contains
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

Using document keys:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentKeyMulti}
  ~ var assertEqual = require("jsunity").jsUnity.assertions.assertEqual;
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );

    var url = "/_api/document/" + cn;

    var body = [ "1", "2" ];
    var response = logCurlRequest('DELETE', url, body);

    assert(response.code === 200);
    assertEqual(JSON.parse(response.body), documents);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using document identifiers:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentIdentifierMulti}
  ~ var assertEqual = require("jsunity").jsUnity.assertions.assertEqual;
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );

    var url = "/_api/document/" + cn;

    var body = [ "products/1", "products/2" ];
    var response = logCurlRequest('DELETE', url, body);

    assert(response.code === 200);
    assertEqual(JSON.parse(response.body), documents);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using objects with document keys:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentObjectMulti}
  ~ var assertEqual = require("jsunity").jsUnity.assertions.assertEqual;
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );

    var url = "/_api/document/" + cn;

    var body = [ { "_key": "1" }, { "_key": "2" } ];
    var response = logCurlRequest('DELETE', url, body);

    assert(response.code === 200);
    assertEqual(JSON.parse(response.body), documents);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown documents:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentUnknownMulti}
    var cn = "products";
    db._drop(cn);
    db._drop("other");
    db._create(cn, { waitForSync: true });
    db._create("other", { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );
    db.products.remove(documents);
    db.other.save( { "_key": "2" } );

    var url = "/_api/document/" + cn;

    var body = [ "1", "other/2" ];
    var response = logCurlRequest('DELETE', url, body);
    var parsedBody = JSON.parse(response.body);

    assert(response.code === 202);
  | parsedBody.forEach(function(doc) {
  |   assert(doc.error === true);
  |   assert(doc.errorNum === 1202);
    });

    logJsonResponse(response);
  ~ db._drop(cn);
  ~ db._drop("other");
@END_EXAMPLE_ARANGOSH_RUN

Check revisions:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentRevMulti}
  ~ var assertEqual = require("jsunity").jsUnity.assertions.assertEqual;
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );

    var url = "/_api/document/" + cn + "?ignoreRevs=false";
  | var body = [
  |   { "_key": "1", "_rev": documents[0]._rev },
  |   { "_key": "2", "_rev": documents[1]._rev }
    ];

    var response = logCurlRequest('DELETE', url, body);

    assert(response.code === 200);
    assertEqual(JSON.parse(response.body), documents);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Revision conflict:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentRevConflictMulti}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

  | var documents = db.products.save( [
  |   { "_key": "1", "type": "tv" },
  |   { "_key": "2", "type": "cookbook" }
    ] );

    var url = "/_api/document/" + cn + "?ignoreRevs=false";
  | var body = [
  |   { "_key": "1", "_rev": "non-matching revision" },
  |   { "_key": "2", "_rev": "non-matching revision" }
    ];

    var response = logCurlRequest('DELETE', url, body);
    var parsedBody = JSON.parse(response.body);

    assert(response.code === 202);
  | parsedBody.forEach(function(doc) {
  |   assert(doc.error === true);
  |   assert(doc.errorNum === 1200);
    });

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
