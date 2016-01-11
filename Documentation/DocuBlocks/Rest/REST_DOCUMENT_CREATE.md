////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock REST_DOCUMENT_CREATE
/// @brief creates a document
///
/// @RESTHEADER{POST /_api/document,Create document}
///
/// @RESTALLBODYPARAM{document,json,required}
/// A JSON representation of the document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTQUERYPARAM{createCollection,boolean,optional}
/// If this parameter has a value of *true* or *yes*, then the collection is
/// created if it does not yet exist. Other values will be ignored so the
/// collection must be present for the operation to succeed.
///
/// **Note**: this flag is not supported in a cluster. Using it will result in an
/// error.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been synced to disk.
///
/// @RESTDESCRIPTION
/// Creates a new document in the collection named *collection*.  A JSON
/// representation of the document must be passed as the body of the POST
/// request.
///
/// If the document was created successfully, then the "Location" header
/// contains the path to the newly created document. The "ETag" header field
/// contains the revision of the document.
///
/// The body of the response contains a JSON object with the following
/// attributes:
///
/// - *_id* contains the document handle of the newly created document
/// - *_key* contains the document key
/// - *_rev* contains the document revision
///
/// If the collection parameter *waitForSync* is *false*, then the call returns
/// as soon as the document has been accepted. It will not wait until the
/// document has been synced to disk.
///
/// Optionally, the query parameter *waitForSync* can be used to force
/// synchronization of the document creation operation to disk even in case that
/// the *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* query parameter can be used to force synchronization of just
/// this specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to *false*,
/// then the collection's default *waitForSync* behavior is applied. The
/// *waitForSync* query parameter cannot be used to disable synchronization for
/// collections that have a default *waitForSync* value of *true*.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created successfully and *waitForSync* was
/// *true*.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created successfully and *waitForSync* was
/// *false*.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Create a document in a collection named *products*. Note that the
/// revision identifier might or might not by equal to the auto-generated
/// key.
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostCreate1}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: true });
///
///     var url = "/_api/document?collection=" + cn;
///     var body = '{ "Hello": "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Create a document in a collection named *products* with a collection-level
/// *waitForSync* value of *false*.
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostAccept1}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: false });
///
///     var url = "/_api/document?collection=" + cn;
///     var body = '{ "Hello": "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Create a document in a collection with a collection-level *waitForSync*
/// value of *false*, but using the *waitForSync* query parameter.
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostWait1}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: false });
///
///     var url = "/_api/document?collection=" + cn + "&waitForSync=true";
///     var body = '{ "Hello": "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Create a document in a new, named collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostCreate2}
///     var cn = "products";
///     db._drop(cn);
///
///     var url = "/_api/document?collection=" + cn + "&createCollection=true";
///     var body = '{ "Hello": "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown collection name
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostUnknownCollection1}
///     var cn = "products";
///     db._drop(cn);
///
///     var url = "/_api/document?collection=" + cn;
///     var body = '{ "Hello": "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Illegal document
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerPostBadJson1}
///     var cn = "products";
///     db._drop(cn);
///
///     var url = "/_api/document?collection=" + cn;
///     var body = '{ 1: "World" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////