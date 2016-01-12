////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock REST_DOCUMENT_READ_HEAD
/// @brief reads a single document head
///
/// @RESTHEADER{HEAD /_api/document/{document-handle},Read document header}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The handle of the document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally fetch a document based on a target revision id by
/// using the *rev* query parameter.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-None-Match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. If the current document revision is different to the specified etag,
/// an *HTTP 200* response is returned. If the current document revision is
/// identical to the specified etag, then an *HTTP 304* is returned.
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally fetch a document based on a target revision id by
/// using the *if-match* HTTP header.
///
/// @RESTDESCRIPTION
/// Like *GET*, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the document was found
///
/// @RESTRETURNCODE{304}
/// is returned if the "If-None-Match" header is given and the document has
/// same version
///*
/// @RESTRETURNCODE{404}
/// is returned if the document or collection was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or *rev* is given and the found
/// document has a different version. The response will also contain the found
/// document's current revision in the *etag* header.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerReadDocumentHead}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var document = db.products.save({"hello":"world"});
///     var url = "/_api/document/" + document._id;
///
///     var response = logCurlRequest('HEAD', url);
///
///     assert(response.code === 200);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////