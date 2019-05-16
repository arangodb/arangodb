@startDocuBlock post_create_document
@brief creates documents

@RESTHEADER{POST /_api/document/{collection}, Create document, insertDocument}

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
The *collection* in which the collection is to be created.

@RESTALLBODYPARAM{data,json,required}
A JSON representation of a single document or of an array of documents.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,optional}
The name of the collection. This is only for backward compatibility.
In ArangoDB versions < 3.0, the URL path was */_api/document* and
this query parameter was required. This combination still works, but
the recommended way is to specify the collection in the URL path.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until document has been synced to disk.

@RESTQUERYPARAM{returnNew,boolean,optional}
Additionally return the complete new document under the attribute *new*
in the result.

@RESTQUERYPARAM{returnOld,boolean,optional}
Additionally return the complete old document under the attribute *old*
in the result. Only available if the overwrite option is used.

@RESTQUERYPARAM{silent,boolean,optional}
If set to *true*, an empty object will be returned as response. No meta-data 
will be returned for the created document. This option can be used to
save some network traffic.

@RESTQUERYPARAM{overwrite,boolean,optional}
If set to *true*, the insert becomes a replace-insert. If a document with the
same *_key* already exists the new document is not rejected with unique
constraint violated but will replace the old document.

@RESTDESCRIPTION
Creates a new document from the document given in the body, unless there
is already a document with the *_key* given. If no *_key* is given, a new
unique *_key* is generated automatically.

The body can be an array of documents, in which case all
documents in the array are inserted with the same semantics as for a
single document. The result body will contain a JSON array of the
same length as the input array, and each entry contains the result
of the operation for the corresponding input. In case of an error
the entry is a document with attributes *error* set to *true* and
errorCode set to the error code that has happened.

Possibly given *_id* and *_rev* attributes in the body are always ignored,
the URL part or the query parameter collection respectively counts.

If the document was created successfully, then the *Location* header
contains the path to the newly created document. The *Etag* header field
contains the revision of the document. Both are only set in the single
document case.

If *silent* is not set to *true*, the body of the response contains a 
JSON object (single document case) with the following attributes:

  - *_id* contains the document handle of the newly created document
  - *_key* contains the document key
  - *_rev* contains the document revision

In the multi case the body is an array of such objects.

If the collection parameter *waitForSync* is *false*, then the call
returns as soon as the document has been accepted. It will not wait
until the documents have been synced to disk.

Optionally, the query parameter *waitForSync* can be used to force
synchronization of the document creation operation to disk even in
case that the *waitForSync* flag had been disabled for the entire
collection. Thus, the *waitForSync* query parameter can be used to
force synchronization of just this specific operations. To use this,
set the *waitForSync* parameter to *true*. If the *waitForSync*
parameter is not specified or set to *false*, then the collection's
default *waitForSync* behavior is applied. The *waitForSync* query
parameter cannot be used to disable synchronization for collections
that have a default *waitForSync* value of *true*.

If the query parameter *returnNew* is *true*, then, for each
generated document, the complete new document is returned under
the *new* attribute in the result.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the documents were created successfully and
*waitForSync* was *true*.

@RESTRETURNCODE{202}
is returned if the documents were created successfully and
*waitForSync* was *false*.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation
of one document or an array of documents. The response body contains
an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.
The response body contains an error document in this case.

@RESTRETURNCODE{409}
is returned in the single document case if a document with the
same qualifiers in an indexed attribute conflicts with an already
existing document and thus violates that unique constraint. The
response body contains an error document in this case. In the array
case only 201 or 202 is returned, but if an error occurred, the
additional HTTP header *X-Arango-Error-Codes* is set, which
contains a map of the error codes that occurred together with their
multiplicities, as in: *1205:10,1210:17* which means that in 10
cases the error 1205 "illegal document handle" and in 17 cases the
error 1210 "unique constraint violated" has happened.

@EXAMPLES

Create a document in a collection named *products*. Note that the
revision identifier might or might not by equal to the auto-generated
key.

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostCreate1}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

    var url = "/_api/document/" + cn;
    var body = '{ "Hello": "World" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Create a document in a collection named *products* with a collection-level
*waitForSync* value of *false*.

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostAccept1}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: false });

    var url = "/_api/document/" + cn;
    var body = '{ "Hello": "World" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Create a document in a collection with a collection-level *waitForSync*
value of *false*, but using the *waitForSync* query parameter.

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostWait1}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: false });

    var url = "/_api/document/" + cn + "?waitForSync=true";
    var body = '{ "Hello": "World" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Unknown collection name

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostUnknownCollection1}
    var cn = "products";

    var url = "/_api/document/" + cn;
    var body = '{ "Hello": "World" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Illegal document

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostBadJson1}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn;
    var body = '{ 1: "World" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 400);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Insert multiple documents:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostMulti1}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn;
    var body = '[{"Hello":"Earth"}, {"Hello":"Venus"}, {"Hello":"Mars"}]';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Use of returnNew:

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostMulti2}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/document/" + cn + "?returnNew=true";
    var body = '{"Hello":"World"}';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostOverwrite}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });

    var url = "/_api/document/" + cn;
    var body = '{ "Hello": "World", "_key" : "lock" }';
    var response = logCurlRequest('POST', url, body);
    // insert
    assert(response.code === 201);
    logJsonResponse(response);

    body = '{ "Hello": "Universe", "_key" : "lock" }';
    url = "/_api/document/" + cn + "?overwrite=true";
    response = logCurlRequest('POST', url, body);
    // insert same key
    assert(response.code === 201);
    logJsonResponse(response);

    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

