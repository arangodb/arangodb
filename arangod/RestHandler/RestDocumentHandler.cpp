////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestDocumentHandler.h"
#include "Basics/conversions.h"
#include "Basics/json-utilities.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDocumentHandler::RestDocumentHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}

HttpHandler::status_t RestDocumentHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE:
      deleteDocument();
      break;
    case HttpRequest::HTTP_REQUEST_GET:
      readDocument();
      break;
    case HttpRequest::HTTP_REQUEST_HEAD:
      checkDocument();
      break;
    case HttpRequest::HTTP_REQUEST_POST:
      createDocument();
      break;
    case HttpRequest::HTTP_REQUEST_PUT:
      replaceDocument();
      break;
    case HttpRequest::HTTP_REQUEST_PATCH:
      updateDocument();
      break;

    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
    }
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_CREATE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (!suffix.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  char const* collection = _request->value("collection", found);

  if (!found || *collection == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }
  std::string collectionName(collection);

  bool parseSuccess = true;
  // copy default options
  VPackOptions options = VPackOptions::Defaults;
  options.checkAttributeUniqueness = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
  if (!parseSuccess) {
    return false;
  }


  /* TODO
  if (!checkCreateCollection(collection, getCollectionType())) {
    return false;
  }
  */

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                                          collection, TRI_TRANSACTION_WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  arangodb::OperationOptions opOptions;
  opOptions.waitForSync = extractWaitForSync();
  VPackSlice body = parsedBody->slice();
  arangodb::OperationResult result = trx.insert(collectionName, body, opOptions);

  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = trx.finish(result.code);

  // .............................................................................
  // outside write transaction
  // .............................................................................
  
  if (result.failed()) {
    generateTransactionError(result);
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  generateSaved(result, collectionName, TRI_col_type_e(trx.getCollectionType(collectionName)));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocumentCoordinator(
    char const* collection, bool waitForSync, VPackSlice const& document) {
  std::string const& dbname = _request->databaseName();
  std::string const collname(collection);
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> headers =
      arangodb::getForwardableRequestHeaders(_request);
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int res = arangodb::createDocumentOnCoordinator(
      dbname, collname, waitForSync, document, headers, responseCode,
      resultHeaders, resultBody);

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  // Essentially return the response we got from the DBserver, be it
  // OK or an error:
  createResponse(responseCode);
  arangodb::mergeResponseHeaders(_response, resultHeaders);
  _response->body().appendText(resultBody.c_str(), resultBody.size());
  return responseCode >= arangodb::rest::HttpResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument() {
  size_t const len = _request->suffix().size();

  switch (len) {
    case 0:
      return readAllDocuments();

    case 1:
      generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                    "expecting GET /_api/document/<document-handle>");
      return false;

    case 2:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<document-handle> or GET "
                    "/_api/document?collection=<collection-name>");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument(bool generateBody) {
  std::vector<std::string> const& suffix = _request->suffix();

  // split the document reference
  std::string const& collection = suffix[0];
  std::string const& key = suffix[1];

  // check for an etag
  bool isValidRevision;
  TRI_voc_rid_t const ifNoneRid =
      extractRevision("if-none-match", 0, isValidRevision);
  if (!isValidRevision) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  TRI_voc_rid_t const ifRid =
      extractRevision("if-match", "rev", isValidRevision);
  if (!isValidRevision) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
    if (ifRid != 0) {
      builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(ifRid));
    }
  }
  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                                          collection, TRI_TRANSACTION_READ);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  OperationOptions options;
  OperationResult result = trx.document(collection, search, options);

  res = trx.finish(result.code);

  if(!result.successful()) {
    if (result.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(collection, (TRI_voc_key_t)key.c_str());
      return false;
    } else if (ifRid != 0 && result.code == TRI_ERROR_ARANGO_CONFLICT) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(collection, res, (TRI_voc_key_t)key.c_str());
    }
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res, (TRI_voc_key_t)key.c_str());
    return false;
  }

  TRI_voc_rid_t const rid =
      VelocyPackHelper::getNumericValue<TRI_voc_rid_t>(
          result.slice(), TRI_VOC_ATTRIBUTE_REV, 0);
  if (ifNoneRid != 0 && ifNoneRid == rid) {
    generateNotModified(rid);
  } else {
    // copy default options
    VPackOptions options = VPackOptions::Defaults;
    options.customTypeHandler = result.customTypeHandler;
    generateDocument(result.slice(), generateBody, &options);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::getDocumentCoordinator(std::string const& collname,
                                                 std::string const& key,
                                                 bool generateBody) {
  std::string const& dbname = _request->databaseName();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>(
          arangodb::getForwardableRequestHeaders(_request)));
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  TRI_voc_rid_t rev = 0;
  bool found;
  char const* revstr = _request->value("rev", found);
  if (found) {
    rev = StringUtils::uint64(revstr);
  }

  int error = arangodb::getDocumentOnCoordinator(
      dbname, collname, key, rev, headers, generateBody, responseCode,
      resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collname, error);
    return false;
  }
  // Essentially return the response we got from the DBserver, be it
  // OK or an error:
  createResponse(responseCode);
  arangodb::mergeResponseHeaders(_response, resultHeaders);

  if (!generateBody) {
    // a head request...
    _response->headResponse(
        (size_t)StringUtils::uint64(resultHeaders["content-length"]));
  } else {
    _response->body().appendText(resultBody.c_str(), resultBody.size());
  }
  return responseCode >= arangodb::rest::HttpResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_ALL
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readAllDocuments() {
  bool found;
  std::string collection = _request->value("collection", found);
  std::string returnType = _request->value("type", found);

  if (returnType.empty()) {
    returnType = "path";
  }

  if (ServerState::instance()->isCoordinator()) {
    return getAllDocumentsCoordinator(collection, returnType);
  }

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                                          collection, TRI_TRANSACTION_READ);

  std::vector<std::string> ids;

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();

  res = trx.all(trx.trxCollection(), ids, true);

  TRI_col_type_e type = trx.documentCollection()->_info.type();

  res = trx.finish(res);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  std::string prefix;

  if (returnType == "key") {
    prefix = "";
  } else if (returnType == "id") {
    prefix = trx.resolver()->getCollectionName(cid) + "/";
  } else {
    // default return type: paths to documents
    if (type == TRI_COL_TYPE_EDGE) {
      prefix = "/_db/" + _request->databaseName() + EDGE_PATH + '/' +
               trx.resolver()->getCollectionName(cid) + '/';
    } else {
      prefix = "/_db/" + _request->databaseName() + DOCUMENT_PATH + '/' +
               trx.resolver()->getCollectionName(cid) + '/';
    }
  }

  try {
    // generate result
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("documents", VPackValue(VPackValueType::Array));

    for (auto const& id : ids) {
      std::string v(prefix);
      v.append(id);
      result.add(VPackValue(v));
    }
    result.close();
    result.close();

    VPackSlice s = result.slice();
    // and generate a response
    generateResult(s);
  } catch (...) {
    // Ignore the error
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::getAllDocumentsCoordinator(
    std::string const& collname, std::string const& returnType) {
  std::string const& dbname = _request->databaseName();

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::string contentType;
  std::string resultBody;

  int error = arangodb::getAllDocumentsOnCoordinator(
      dbname, collname, returnType, responseCode, contentType, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collname, error);
    return false;
  }
  // Return the response we got:
  createResponse(responseCode);
  _response->setContentType(contentType);
  _response->body().appendText(resultBody.c_str(), resultBody.size());
  return responseCode >= arangodb::rest::HttpResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_HEAD
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting URI /_api/document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_REPLACE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::replaceDocument() { return modifyDocument(false); }

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_UPDATE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument() { return modifyDocument(true); }

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceDocument and updateDocument
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::modifyDocument(bool isPatch) {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    std::string msg("expecting ");
    msg.append(isPatch ? "PATCH" : "PUT");
    msg.append(" /_api/document/<document-handle>");

    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return false;
  }

  // split the document reference
  std::string const& collection = suffix[0];
  std::string const& key = suffix[1];

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  VPackSlice body = parsedBody->slice();

  if (!body.isObject()) {
    generateTransactionError(collection,
                             TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    return false;
  }

  // extract the revision
  bool isValidRevision;
  TRI_voc_rid_t const revision =
      extractRevision("if-match", "rev", isValidRevision);
  if (!isValidRevision) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  // extract or chose the update policy
  TRI_doc_update_policy_e const policy = extractUpdatePolicy();

  // extract or chose the update policy
  OperationOptions opOptions;
  opOptions.waitForSync = extractWaitForSync();

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
    if (revision != 0 && policy != TRI_DOC_UPDATE_LAST_WRITE) {
      builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(revision));
    }
  }

  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                                          collection, TRI_TRANSACTION_WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  OperationResult result(TRI_ERROR_NO_ERROR);
  if (isPatch) {
    // patching an existing document
    bool found;
    char const* valueStr = _request->value("keepNull", found);
    if (!found || StringUtils::boolean(valueStr)) {
      // default: null values are saved as Null
      opOptions.keepNull = true;
    } else {
      // delete null attributes
      opOptions.keepNull = false;
    }

    valueStr = _request->value("mergeObjects", found);
    if (!found || StringUtils::boolean(valueStr)) {
      // the default is true
      opOptions.mergeObjects = true;
    } else {
      opOptions.mergeObjects = false;
    }

    result = trx.update(collection, search, body, opOptions);
  } else {
    // replacing an existing document
    result = trx.replace(collection, search, body, opOptions);
  }

  res = trx.finish(result.code);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (result.failed()) {
    generateTransactionError(result);
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // This should not occur. Updated worked but commit failed
    generateTransactionError(collection, res, (TRI_voc_key_t)key.c_str(), 0);
    return false;
  }

  generateSaved(result, collection, TRI_col_type_e(trx.getCollectionType(collection)));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modifies a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::modifyDocumentCoordinator(
    std::string const& collname, std::string const& key,
    TRI_voc_rid_t const rev, TRI_doc_update_policy_e policy, bool waitForSync,
    bool isPatch, VPackSlice const& document) {
  std::string const& dbname = _request->databaseName();
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>(
          arangodb::getForwardableRequestHeaders(_request)));
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  bool keepNull = true;
  if (!strcmp(_request->value("keepNull"), "false")) {
    keepNull = false;
  }
  bool mergeObjects = true;
  if (TRI_EqualString(_request->value("mergeObjects"), "false")) {
    mergeObjects = false;
  }

  int error = arangodb::modifyDocumentOnCoordinator(
      dbname, collname, key, rev, policy, waitForSync, isPatch, keepNull,
      mergeObjects, document, headers, responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collname, error);
    return false;
  }

  // Essentially return the response we got from the DBserver, be it
  // OK or an error:
  createResponse(responseCode);
  arangodb::mergeResponseHeaders(_response, resultHeaders);
  _response->body().appendText(resultBody.c_str(), resultBody.size());
  return responseCode >= arangodb::rest::HttpResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_DELETE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<document-handle>");
    return false;
  }

  // split the document reference
  std::string const& collection = suffix[0];
  std::string const& key = suffix[1];

  // extract the revision
  bool isValidRevision;
  TRI_voc_rid_t const revision =
      extractRevision("if-match", "rev", isValidRevision);
  if (!isValidRevision) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }
  OperationOptions opOptions;

  TRI_doc_update_policy_e const policy = extractUpdatePolicy();
  if (policy == TRI_DOC_UPDATE_ILLEGAL) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "policy must be 'error' or 'last'");
    return false;
  }

  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                                          collection, TRI_TRANSACTION_WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }


  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
    if (revision != 0 && policy != TRI_DOC_UPDATE_LAST_WRITE) {
      builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(revision));
    }
  }
  VPackSlice search = builder.slice();

  OperationResult result = trx.remove(collection, search, opOptions);

  res = trx.finish(result.code);

  if(!result.successful()) {
    generateTransactionError(result);
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res, (TRI_voc_key_t)key.c_str());
    return false;
  }

  generateDeleted(result, collection, TRI_col_type_e(trx.getCollectionType(collection)));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocumentCoordinator(
    std::string const& collname, std::string const& key,
    TRI_voc_rid_t const rev, TRI_doc_update_policy_e policy, bool waitForSync) {
  std::string const& dbname = _request->databaseName();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>(
          arangodb::getForwardableRequestHeaders(_request)));
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int error = arangodb::deleteDocumentOnCoordinator(
      dbname, collname, key, rev, policy, waitForSync, headers, responseCode,
      resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collname, error);
    return false;
  }
  // Essentially return the response we got from the DBserver, be it
  // OK or an error:
  createResponse(responseCode);
  arangodb::mergeResponseHeaders(_response, resultHeaders);
  _response->body().appendText(resultBody.c_str(), resultBody.size());
  return responseCode >= arangodb::rest::HttpResponse::BAD;
}
