////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock REST_DOCUMENT_REPLACE
/// @brief replaces a document
///
/// @RESTHEADER{PUT /_api/document/{document-handle},Replace document}
///
/// @RESTALLBODYPARAM{document,json,required}
/// A JSON representation of the new document.
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The handle of the document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been synced to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the *rev* query parameter.
///
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the *policy* parameter (see below).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the *if-match* HTTP header.
///
/// @RESTDESCRIPTION
/// Completely updates (i.e. replaces) the document identified by *document-handle*.
/// If the document exists and can be updated, then a *HTTP 201* is returned
/// and the "ETag" header field contains the new revision of the document.
///
/// If the new document passed in the body of the request contains the
/// *document-handle* in the attribute *_id* and the revision in *_rev*,
/// these attributes will be ignored. Only the URI and the "ETag" header are
/// relevant in order to avoid confusion when using proxies.
///
///
/// Optionally, the query parameter *waitForSync* can be used to force
/// synchronization of the document replacement operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* query parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* query parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision. The attribute *_id* contains the known
/// *document-handle* of the updated document, *_key* contains the key which 
/// uniquely identifies a document in a given collection, and the attribute *_rev*
/// contains the new document revision.
///
/// If the document does not exist, then a *HTTP 404* is returned and the
/// body of the response contains an error document.
///
/// There are two ways for specifying the targeted document revision id for
/// conditional replacements (i.e. replacements that will only be executed if
/// the revision id found in the database matches the document revision id specified
/// in the request):
/// - specifying the target revision in the *rev* URL query parameter
/// - specifying the target revision in the *if-match* HTTP header
///
///
/// Specifying a target revision is optional, however, if done, only one of the
/// described mechanisms must be used (either the *rev* query parameter or the
/// *if-match* HTTP header).
/// Regardless which mechanism is used, the parameter needs to contain the target
/// document revision id as returned in the *_rev* attribute of a document or
/// by an HTTP *etag* header.
///
/// For example, to conditionally replace a document based on a specific revision
/// id, you can use the following request:
///
///
/// `PUT /_api/document/document-handle?rev=etag`
///
///
/// If a target revision id is provided in the request (e.g. via the *etag* value
/// in the *rev* URL query parameter above), ArangoDB will check that
/// the revision id of the document found in the database is equal to the target
/// revision id provided in the request. If there is a mismatch between the revision
/// id, then by default a *HTTP 412* conflict is returned and no replacement is
/// performed.
///
///
/// The conditional update behavior can be overridden with the *policy* URL query parameter:
///
///
/// `PUT /_api/document/document-handle?policy=policy`
///
///
/// If *policy* is set to *error*, then the behavior is as before: replacements
/// will fail if the revision id found in the database does not match the target
/// revision id specified in the request.
///
/// If *policy* is set to *last*, then the replacement will succeed, even if the
/// revision id found in the database does not match the target revision id specified
/// in the request. You can use the *last* *policy* to force replacements.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was replaced successfully and *waitForSync* was
/// *true*.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was replaced successfully and *waitForSync* was
/// *false*.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection or the document was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or *rev* is given and the found
/// document has a different version. The response will also contain the found
/// document's current revision in the *_rev* attribute. Additionally, the
/// attributes *_id* and *_key* will be returned.
///
/// @EXAMPLES
///
/// Using a document handle
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocument}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     var url = "/_api/document/" + document._id;
///
///     var response = logCurlRequest('PUT', url, '{"Hello": "you"}');
///
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown document handle
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentUnknownHandle}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     db.products.remove(document._id);
///     var url = "/_api/document/" + document._id;
///
///     var response = logCurlRequest('PUT', url, "{}");
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Produce a revision conflict
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentIfMatchOther}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     var document2 = db.products.save({"hello2":"world"});
///     var url = "/_api/document/" + document._id;
///     var headers = {"If-Match":  "\"" + document2._rev + "\""};
///
///     var response = logCurlRequest('PUT', url, '{"other":"content"}', headers);
///
///     assert(response.code === 412);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Last write wins
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentIfMatchOtherLastWriteWins}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     var document2 = db.products.replace(document._id,{"other":"content"});
///     var url = "/_api/document/products/" + document._rev + "?policy=last";
///     var headers = {"If-Match":  "\"" + document2._rev + "\""};
///
///     var response = logCurlRequest('PUT', url, "{}", headers);
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Alternative to header fields
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerUpdateDocumentRevOther}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     var document2 = db.products.save({"hello2":"world"});
///     var url = "/_api/document/" + document._id + "?rev=" + document2._rev;
///
///     var response = logCurlRequest('PUT', url, '{"other":"content"}');
///
///     assert(response.code === 412);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////