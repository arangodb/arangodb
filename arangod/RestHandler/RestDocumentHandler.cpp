////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestDocumentHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/string-buffer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "Utils/Barrier.h"
#include "Utilities/ResourceHolder.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestDocumentHandler::RestDocumentHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestDocumentHandler::queue () const {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestDocumentHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_PATH + " [create]");
  static LoggerData::Task const logRead(DOCUMENT_PATH + " [read]");
  static LoggerData::Task const logUpdate(DOCUMENT_PATH + " [update]");
  static LoggerData::Task const logDelete(DOCUMENT_PATH + " [delete]");
  static LoggerData::Task const logHead(DOCUMENT_PATH + " [head]");
  static LoggerData::Task const logOptions(DOCUMENT_PATH + " [options]");
  static LoggerData::Task const logPatch(DOCUMENT_PATH + " [patch]");
  static LoggerData::Task const logIllegal(DOCUMENT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: task = &logDelete; break;
    case HttpRequest::HTTP_REQUEST_GET: task = &logRead; break;
    case HttpRequest::HTTP_REQUEST_HEAD: task = &logHead; break;
    case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
    case HttpRequest::HTTP_REQUEST_OPTIONS: task = &logOptions; break;
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    case HttpRequest::HTTP_REQUEST_PUT: task = &logUpdate; break;
    case HttpRequest::HTTP_REQUEST_PATCH: task = &logUpdate; break;
  }

  _timing << *task;
#ifdef TRI_ENABLE_LOGGER
  // if logger is not activated, the compiler will complain, so enclose it in ifdef
  LOGGER_REQUEST_IN_START_I(_timing, "");
#endif

  // execute one of the CRUD methods
  bool res = false;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: res = deleteDocument(); break;
    case HttpRequest::HTTP_REQUEST_GET:    res = readDocument(); break;
    case HttpRequest::HTTP_REQUEST_HEAD:   res = checkDocument(); break;
    case HttpRequest::HTTP_REQUEST_POST:   res = createDocument(); break;
    case HttpRequest::HTTP_REQUEST_PUT:    res = replaceDocument(); break;
    case HttpRequest::HTTP_REQUEST_PATCH:  res = updateDocument(); break;

    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
    }
  }

  _timingResult = res ? RES_ERR : RES_OK;

  // this handler is done
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
///
/// @RESTHEADER{POST /_api/document,creates a document}
///
/// @RESTBODYPARAM{document,json,required}
/// A JSON representation of document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTQUERYPARAM{createCollection,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then the collection is
/// created if it does not yet exist. Other values will be ignored so the
/// collection must be present for the operation to succeed.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTDESCRIPTION
/// Creates a new document in the collection named `collection`.  A JSON
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
/// - `_id` contains the document handle of the newly created document
/// - `_key` contains the document key
/// - `_rev` contains the document revision
///
/// If the collection parameter `waitForSync` is `false`, then the call returns
/// as soon as the document has been accepted. It will not wait, until the
/// documents has been sync to disk.
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document creation operation to disk even in case that
/// the `waitForSync` flag had been disabled for the entire collection.  Thus,
/// the `waitForSync` URL parameter can be used to force synchronisation of just
/// this specific operations. To use this, set the `waitForSync` parameter to
/// `true`. If the `waitForSync` parameter is not specified or set to `false`,
/// then the collection's default `waitForSync` behavior is applied. The
/// `waitForSync` URL parameter cannot be used to disable synchronisation for
/// collections that have a default `waitForSync` value of `true`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Create a document given a collection named `products`. Note that the
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Create a document in a collection named `products` with a collection-level
/// `waitForSync` value of `false`.
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Create a document in a collection with a collection-level `waitForSync`
/// value of `false`, but using the `waitForSync` URL parameter.
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown collection name:
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
/// Illegal document:
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
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  char const* collection = _request->value("collection", found);

  if (! found || *collection == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  const bool waitForSync = extractWaitForSync();

  // auto-ptr that will free JSON data when scope is left
  ResourceHolder holder;

  TRI_json_t* json = parseJsonBody();
  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, json)) {
    return false;
  }

  if (! checkCreateCollection(collection, getCollectionType())) {
    return false;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    generateTransactionError(collection, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, 1> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  if (trx.primaryCollection()->base._info._type == TRI_COL_TYPE_EDGE) {
    // check if we are inserting with the DOCUMENT handler into an EDGE collection
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "must not use the document handler to create an edge");
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();

  TRI_doc_mptr_t document;
  res = trx.createDocument(&document, json, waitForSync);
  const bool wasSynchronous = trx.synchronous();
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  assert(document._key != 0);

  // generate result
  if (wasSynchronous) {
    generateCreated(cid, document._key, document._rid);
  }
  else {
    generateAccepted(cid, document._key, document._rid);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument () {
  const size_t len = _request->suffix().size();

  switch (len) {
    case 0:
      return readAllDocuments();

    case 1:
      generateError(HttpResponse::BAD,
                    TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                    "expecting GET /_api/document/<document-handle>");
      return false;

    case 2:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<document-handle> or GET /_api/document?collection=<collection-name>");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @RESTHEADER{GET /_api/document/`document-handle`,reads a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-None-Match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a `HTTP 304` is returned.
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Returns the document identified by `document-handle`. The returned
/// document contains two special attributes: `_id` containing the document
/// handle and `_rev` containing the revision.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the document was found
///
/// @RESTRETURNCODE{404}
/// is returned if the document or collection was not found
///
/// @RESTRETURNCODE{304}
/// is returned if the "If-None-Match" header is given and the document has
/// same version
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
/// @EXAMPLES
///
/// Use a document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocument}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     var document = db.products.save({"hello":"world"});
///     var url = "/_api/document/" + document._id;
/// 
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Use a document handle and an etag:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocumentIfNoneMatch}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     var document = db.products.save({"hello":"world"});
///     var url = "/_api/document/" + document._id;
///     var headers = {"If-None-Match":  "\"" + document._rev + "\""};
/// 
///     var response = logCurlRequest('GET', url, "", headers);
/// 
///     assert(response.code === 304);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocumentUnknownHandle}
///     var url = "/_api/document/products/unknownhandle";
/// 
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 404);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument (bool generateBody) {
  vector<string> const& suffix = _request->suffix();

  // split the document reference
  const string& collection = suffix[0];
  const string& key = suffix[1];

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction<StandaloneTransaction<RestTransactionContext> > trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();
  TRI_doc_mptr_t document;

  res = trx.read(&document, key);

  TRI_primary_collection_t* primary = trx.primaryCollection();
  assert(primary != 0);
  TRI_shaper_t* shaper = primary->_shaper;

  // register a barrier. will be destroyed automatically
  Barrier barrier(primary);

  res = trx.finish(res);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res, (TRI_voc_key_t) key.c_str());
    return false;
  }

  if (document._key == 0 || document._data == 0) {
    generateDocumentNotFound(cid, (TRI_voc_key_t) key.c_str());
    return false;
  }

  // generate result
  assert(document._key != 0);

  const TRI_voc_rid_t rid = document._rid;
  // check for an etag
  const TRI_voc_rid_t ifNoneRid = extractRevision("if-none-match", 0);
  const TRI_voc_rid_t ifRid = extractRevision("if-match", "rev");

  if (ifNoneRid == 0) {
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(cid, &document, shaper, generateBody);
    }
    else {
      generatePreconditionFailed(cid, document._key, rid);
    }
  }
  else if (ifNoneRid == rid) {
    if (ifRid == 0 || ifRid == rid) {
      generateNotModified(rid);
    }
    else {
      generatePreconditionFailed(cid, document._key, rid);
    }
  }
  else {
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(cid, &document, shaper, generateBody);
    }
    else {
      generatePreconditionFailed(cid, document._key, rid);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all documents from collection
///
/// @RESTHEADER{GET /_api/document,reads all documents from collection}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The Id of the collection.
///
/// @RESTDESCRIPTION
/// Returns a list of all URI for all documents from the collection identified
/// by `collection`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// All went good.
///
/// @RESTRETURNCODE{404}
/// The collection does not exist.
///
/// @EXAMPLES
///
/// Returns a collection.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocumentAll}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     var url = "/_api/document/?collection=" + cn;
/// 
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Collection does not exist.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocumentAllCollectionDoesNotExist}
///     var cn = "doesnotexist";
///     db._drop(cn);
///     var url = "/_api/document/?collection=" + cn;
/// 
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 404);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readAllDocuments () {
  bool found;
  string collection = _request->value("collection", found);

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction<StandaloneTransaction<RestTransactionContext> > trx(_vocbase, _resolver, collection);

  vector<string> ids;

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();

  res = trx.read(ids);

  res = trx.finish(res);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  // generate result
  string result("{ \"documents\" : [\n");

  bool first = true;
  string prefix = '"' + DOCUMENT_PATH + '/' + _resolver.getCollectionName(cid) + '/';

  for (vector<string>::const_iterator i = ids.begin();  i != ids.end();  ++i) {
    // collection names do not need to be JSON-escaped
    // keys do not need to be JSON-escaped
    result += prefix + (*i) + '"';

    if (first) {
      prefix = ",\n" + prefix;
      first = false;
    }
  }

  result += "\n] }";

  // and generate a response
  _response = createResponse(HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");

  _response->body().appendText(result);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @RESTHEADER{HEAD /_api/document/`document-handle`,reads a document header}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for more details).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally get a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Like `GET`, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the document was found
///
/// @RESTRETURNCODE{404}
/// is returned if the document or collection was not found
///
/// @RESTRETURNCODE{304}
/// is returned if the "If-None-Match" header is given and the document has
/// same version
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReadDocumentHead}
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
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting URI /_api/document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @RESTHEADER{PUT /_api/document/`document-handle`,replaces a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
/// 
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for more details).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Completely updates (i.e. replaces) the document identified by `document-handle`.
/// If the document exists and can be updated, then a `HTTP 201` is returned
/// and the "ETag" header field contains the new revision of the document.
///
/// If the new document passed in the body of the request contains the
/// `document-handle` in the attribute `_id` and the revision in `_rev`,
/// these attributes will be ignored. Only the URI and the "ETag" header are
/// relevant in order to avoid confusion when using proxies.
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document replacement operation to disk even in case
/// that the `waitForSync` flag had been disabled for the entire collection.
/// Thus, the `waitForSync` URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the `waitForSync` parameter
/// to `true`. If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the new document revision.
///
/// If the document does not exist, then a `HTTP 404` is returned and the
/// body of the response contains an error document.
///
/// There are two ways for specifying the targeted document revision id for
/// conditional replacements (i.e. replacements that will only be executed if
/// the revision id found in the database matches the document revision id specified
/// in the request):
/// - specifying the target revision in the `rev` URL query parameter
/// - specifying the target revision in the `if-match` HTTP header
///
/// Specifying a target revision is optional, however, if done, only one of the
/// described mechanisms must be used (either the `rev` URL parameter or the
/// `if-match` HTTP header).
/// Regardless which mechanism is used, the parameter needs to contain the target
/// document revision id as returned in the `_rev` attribute of a document or
/// by an HTTP `etag` header.
///
/// For example, to conditionally replace a document based on a specific revision
/// id, you the following request:
/// 
/// - PUT /_api/document/`document-handle`?rev=`etag`
///
/// If a target revision id is provided in the request (e.g. via the `etag` value
/// in the `rev` URL query parameter above), ArangoDB will check that
/// the revision id of the document found in the database is equal to the target
/// revision id provided in the request. If there is a mismatch between the revision
/// id, then by default a `HTTP 412` conflict is returned and no replacement is
/// performed.
///
/// The conditional update behavior can be overriden with the `policy` URL query parameter:
///
/// - PUT /_api/document/`document-handle`?policy=`policy`
///
/// If `policy` is set to `error`, then the behavior is as before: replacements
/// will fail if the revision id found in the database does not match the target
/// revision id specified in the request.
///
/// If `policy` is set to `last`, then the replacement will succeed, even if the
/// revision id found in the database does not match the target revision id specified
/// in the request. You can use the `last` `policy` to force replacements.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if collection or the document was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestUpdateDocument}
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestUpdateDocumentUnknownHandle}
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Produce a revision conflict:
///
/// @EXAMPLE_ARANGOSH_RUN{RestUpdateDocumentIfMatchOther}
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Last write wins:
///
/// @EXAMPLE_ARANGOSH_RUN{RestUpdateDocumentIfMatchOtherLastWriteWins}
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
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Alternative to header field:
///
/// @EXAMPLE_ARANGOSH_RUN{RestUpdateDocumentRevOther}
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
/// @END_EXAMPLE_ARANGOSH_RUN
///////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::replaceDocument () {
  return modifyDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @RESTHEADER{PATCH /_api/document/`document-handle`,patches a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{keepNull,string,optional}
/// If the intention is to delete existing attributes with the patch command, 
/// the URL query parameter `keepNull` can be used with a value of `false`.
/// This will modify the behavior of the patch command to remove any attributes
/// from the existing document that are contained in the patch document with an
/// attribute value of `null`.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally patch a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Partially updates the document identified by `document-handle`.
/// The body of the request must contain a JSON document with the attributes
/// to patch (the patch document). All attributes from the patch document will
/// be added to the existing document if they do not yet exist, and overwritten
/// in the existing document if they do exist there.
///
/// Setting an attribute value to `null` in the patch document will cause a
/// value of `null` be saved for the attribute by default. 
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document update operation to disk even in case
/// that the `waitForSync` flag had been disabled for the entire collection.
/// Thus, the `waitForSync` URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the `waitForSync` parameter
/// to `true`. If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision. The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the new document revision.
///
/// If the document does not exist, then a `HTTP 404` is returned and the
/// body of the response contains an error document.
///
/// You can conditionally update a document based on a target revision id by
/// using either the `rev` URL parameter or the `if-match` HTTP header.
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for details).
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if collection or the document was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
/// @EXAMPLES
///
/// patches an existing document with new content.
///
/// @EXAMPLE_ARANGOSH_RUN{RestPatchDocument}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     var document = db.products.save({"one":"world"});
///     var url = "/_api/document/" + document._id;
/// 
///     var response = logCurlRequest("PATCH", url, { "hello": "world" });
/// 
///     assert(response.code === 202);
/// 
///     logJsonResponse(response);
///     var response2 = logCurlRequest("PATCH", url, { "numbers": { "one": 1, "two": 2, "three": 3, "empty": null } });
///     assert(response2.code === 202);
///     logJsonResponse(response2);
///     var response3 = logCurlRequest("GET", url);
///     assert(response3.code === 200);
///     logJsonResponse(response3);
///     var response4 = logCurlRequest("PATCH", url + "?keepNull=false", { "hello": null, "numbers": { "four": 4 } });
///     assert(response4.code === 202);
///     logJsonResponse(response4);
///     var response5 = logCurlRequest("GET", url);
///     assert(response5.code === 200);
///     logJsonResponse(response5);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument () {
  return modifyDocument(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceDocument and updateDocument
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::modifyDocument (bool isPatch) {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    string msg("expecting ");
    msg.append(isPatch ? "PATCH" : "PUT");
    msg.append(" /_api/document/<document-handle>");

    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  msg);
    return false;
  }

  // split the document reference
  const string& collection = suffix[0];
  const string& key = suffix[1];

  // auto-ptr that will free JSON data when scope is left
  ResourceHolder holder;

  TRI_json_t* json = parseJsonBody();

  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, json)) {
    return false;
  }
  
  if (json->_type != TRI_JSON_ARRAY) {
    generateTransactionError(collection, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    return false;
  }

  // extract the revision
  const TRI_voc_rid_t revision = extractRevision("if-match", "rev");

  // extract or chose the update policy
  const TRI_doc_update_policy_e policy = extractUpdatePolicy();
  const bool waitForSync = extractWaitForSync();

  TRI_doc_mptr_t document;

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, 1> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();
  TRI_voc_rid_t rid = 0;
  TRI_primary_collection_t* primary = trx.primaryCollection();
  assert(primary != 0);
  TRI_shaper_t* shaper = primary->_shaper;

  if (isPatch) {
    // patching an existing document
    bool nullMeansRemove;
    bool found;
    char const* valueStr = _request->value("keepNull", found);
    if (! found || StringUtils::boolean(valueStr)) {
      // default: null values are saved as Null
      nullMeansRemove = false;
    }
    else {
      // delete null attributes
      nullMeansRemove = true;
    }

    // read the existing document
    TRI_doc_mptr_t oldDocument;

    // create a write lock that spans the initial read and the update
    // otherwise the update is not atomic
    trx.lockWrite();

    // do not lock again
    res = trx.read(&oldDocument, key);
    if (res != TRI_ERROR_NO_ERROR) {
      trx.abort();
      generateTransactionError(collection, res, (TRI_voc_key_t) key.c_str(), rid);

      return false;
    }

    if (oldDocument._key == 0 || oldDocument._data == 0) {
      trx.abort();
      generateTransactionError(collection, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, (TRI_voc_key_t) key.c_str(), rid);

      return false;
    }

    TRI_shaped_json_t shapedJson;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, oldDocument._data);
    TRI_json_t* old = TRI_JsonShapedJson(shaper, &shapedJson);

    if (holder.registerJson(shaper->_memoryZone, old)) {
      TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, nullMeansRemove);

      if (holder.registerJson(TRI_UNKNOWN_MEM_ZONE, patchedJson)) {
        // do not acquire an extra lock
        res = trx.updateDocument(key, &document, patchedJson, policy, waitForSync, revision, &rid);
      }
    }
  }
  else {
    // replacing an existing document, using a lock
    res = trx.updateDocument(key, &document, json, policy, waitForSync, revision, &rid);
  }

  const bool wasSynchronous = trx.synchronous();

  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res, (TRI_voc_key_t) key.c_str(), rid);

    return false;
  }

  // generate result
  if (wasSynchronous) {
    generateCreated(cid, (TRI_voc_key_t) key.c_str(), document._rid);
  }
  else {
    generateAccepted(cid, (TRI_voc_key_t) key.c_str(), document._rid);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @RESTHEADER{DELETE /_api/document/`document-handle`,deletes a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// Deletes the document identified by `document-handle`. 
/// 
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for more details).
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the known document revision.
///
/// If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the document was deleted sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was deleted sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection or the document was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the current
/// document has a different version
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDELETEDocument}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: true });
///     var document = db.products.save({"hello":"world"});
///
///     var url = "/_api/document/" + document._id;
///
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown document handle:
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentUnknownHandle}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: true });
///     var document = db.products.save({"hello":"world"});
///     db.products.remove(document._id);
///
///     var url = "/_api/document/" + document._id;
///
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Revision conflict:
///
/// @EXAMPLE_ARANGOSH_RUN{RestDocumentHandlerDeleteDocumentIfMatchOther}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
/// 
///     var document = db.products.save({"hello":"world"});
///     var document2 = db.products.save({"hello2":"world"});
///     var url = "/_api/document/" + document._id;
///     var headers = {"If-Match":  "\"" + document2._rev + "\""};
/// 
///     var response = logCurlRequest('DELETE', url, "", headers);
/// 
///     assert(response.code === 412);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<document-handle>");
    return false;
  }

  // split the document reference
  const string& collection = suffix[0];
  const string& key = suffix[1];

  // extract the revision
  const TRI_voc_rid_t revision = extractRevision("if-match", "rev");

  // extract or choose the update policy
  const TRI_doc_update_policy_e policy = extractUpdatePolicy();
  const bool waitForSync = extractWaitForSync();

  if (policy == TRI_DOC_UPDATE_ILLEGAL) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "policy must be 'error' or 'last'");
    return false;
  }

  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, 1> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();

  TRI_voc_rid_t rid = 0;
  res = trx.deleteDocument(key, policy, waitForSync, revision, &rid);
  
  const bool wasSynchronous = trx.synchronous();
  if (res == TRI_ERROR_NO_ERROR) {
    res = trx.commit();
  }
  else {
    trx.abort();
  }

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res, (TRI_voc_key_t) key.c_str(), rid);
    return false;
  }

  if (wasSynchronous) {
    generateDeleted(cid, (TRI_voc_key_t) key.c_str(), rid);
  }
  else {
    generateAccepted(cid, (TRI_voc_key_t) key.c_str(), rid);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
