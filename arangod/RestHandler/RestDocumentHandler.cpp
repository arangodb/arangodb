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
#include "Rest/GeneralRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDocumentHandler::RestDocumentHandler(GeneralRequest* request)
    : RestVocbaseBaseHandler(request) {}

GeneralHandler::status_t RestDocumentHandler::execute() {
  // extract the sub-request type
  GeneralRequest::RequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case GeneralRequest::HTTP_REQUEST_DELETE:
      deleteDocument();
      break;
    case GeneralRequest::HTTP_REQUEST_GET:
      readDocument();
      break;
    case GeneralRequest::HTTP_REQUEST_HEAD:
      checkDocument();
      break;
    case GeneralRequest::HTTP_REQUEST_POST:
      createDocument();
      break;
    case GeneralRequest::HTTP_REQUEST_PUT:
      replaceDocument();
      break;
    case GeneralRequest::HTTP_REQUEST_PATCH:
      updateDocument();
      break;

    case GeneralRequest::HTTP_REQUEST_ILLEGAL:
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
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  char const* collection = _request->value("collection", found);

  if (!found || *collection == '\0') {
    generateError(GeneralResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const waitForSync = extractWaitForSync();

  bool parseSuccess = true;
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  VPackSlice body = parsedBody->slice();

  if (!body.isObject()) {
    generateTransactionError(collection,
                             TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    return createDocumentCoordinator(collection, waitForSync, body);
  }

  if (!checkCreateCollection(collection, getCollectionType())) {
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<1> trx(new StandaloneTransactionContext(),
                                          _vocbase, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  if (trx.documentCollection()->_info.type() != TRI_COL_TYPE_DOCUMENT) {
    // check if we are inserting with the DOCUMENT handler into a non-DOCUMENT
    // collection
    generateError(GeneralResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();

  TRI_doc_mptr_copy_t mptr;
  res = trx.createDocument(&mptr, body, waitForSync);
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  generateSaved(trx, cid, mptr);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocumentCoordinator(
    char const* collection, bool waitForSync, VPackSlice const& document) {
  std::string const& dbname = _request->databaseName();
  std::string const collname(collection);
  arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
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
  return responseCode >= arangodb::rest::GeneralResponse::BAD;
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
      generateError(GeneralResponse::BAD, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                    "expecting GET /_api/document/<document-handle>");
      return false;

    case 2:
      return readSingleDocument(true);

    default:
      generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
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
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  TRI_voc_rid_t const ifRid =
      extractRevision("if-match", "rev", isValidRevision);
  if (!isValidRevision) {
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    return getDocumentCoordinator(collection, key, generateBody);
  }

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(),
                                          _vocbase, collection);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();
  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  std::string collectionName = collection;
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(cid);
  }
  TRI_doc_mptr_copy_t mptr;

  res = trx.read(&mptr, key);

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_ASSERT(document != nullptr);
  auto shaper = document->getShaper();  // PROTECTED by trx here

  res = trx.finish(res);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  TRI_ASSERT(trx.hasDitch());

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, (TRI_voc_key_t)key.c_str());
    return false;
  }

  if (mptr.getDataPtr() == nullptr) {  // PROTECTED by trx here
    generateDocumentNotFound(trx, cid, (TRI_voc_key_t)key.c_str());
    return false;
  }

  // generate result
  TRI_voc_rid_t const rid = mptr._rid;

  if (ifNoneRid == 0) {
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(trx, cid, mptr, shaper, generateBody);
    } else {
      generatePreconditionFailed(trx, cid, mptr, rid);
    }
  } else if (ifNoneRid == rid) {
    if (ifRid == 0 || ifRid == rid) {
      generateNotModified(rid);
    } else {
      generatePreconditionFailed(trx, cid, mptr, rid);
    }
  } else {
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(trx, cid, mptr, shaper, generateBody);
    } else {
      generatePreconditionFailed(trx, cid, mptr, rid);
    }
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
  arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
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
  return responseCode >= arangodb::rest::GeneralResponse::BAD;
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
  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(),
                                          _vocbase, collection);

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

  res = trx.read(ids);

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

  arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
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
  return responseCode >= arangodb::rest::GeneralResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_HEAD
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
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

    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return false;
  }

  // split the document reference
  std::string const& collection = suffix[0];
  std::string const& key = suffix[1];

  bool parseSuccess = true;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
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
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  // extract or chose the update policy
  TRI_doc_update_policy_e const policy = extractUpdatePolicy();
  bool const waitForSync = extractWaitForSync();

  if (ServerState::instance()->isCoordinator()) {
    return modifyDocumentCoordinator(collection, key, revision, policy,
                                     waitForSync, isPatch, body);
  }

  TRI_doc_mptr_copy_t mptr;

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<1> trx(new StandaloneTransactionContext(),
                                          _vocbase, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();
  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  std::string collectionName = collection;
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(cid);
  }

  TRI_voc_rid_t rid = 0;
  TRI_document_collection_t* document = trx.documentCollection();
  TRI_ASSERT(document != nullptr);
  auto shaper = document->getShaper();  // PROTECTED by trx here

  std::string const&& cidString = StringUtils::itoa(document->_info.planId());

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    generateTransactionError(collectionName, TRI_ERROR_OUT_OF_MEMORY);
    return false;
  }

  if (isPatch) {
    // patching an existing document
    bool nullMeansRemove;
    bool mergeObjects;
    bool found;
    char const* valueStr = _request->value("keepNull", found);
    if (!found || StringUtils::boolean(valueStr)) {
      // default: null values are saved as Null
      nullMeansRemove = false;
    } else {
      // delete null attributes
      nullMeansRemove = true;
    }

    valueStr = _request->value("mergeObjects", found);
    if (!found || StringUtils::boolean(valueStr)) {
      // the default is true
      mergeObjects = true;
    } else {
      mergeObjects = false;
    }

    // read the existing document
    TRI_doc_mptr_copy_t oldDocument;

    // create a write lock that spans the initial read and the update
    // otherwise the update is not atomic
    trx.lockWrite();

    // do not lock again
    res = trx.read(&oldDocument, key);
    if (res != TRI_ERROR_NO_ERROR) {
      trx.abort();
      generateTransactionError(collectionName, res, (TRI_voc_key_t)key.c_str(),
                               rid);
      return false;
    }

    if (oldDocument.getDataPtr() == nullptr) {  // PROTECTED by trx here
      trx.abort();
      generateTransactionError(collectionName,
                               TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                               (TRI_voc_key_t)key.c_str(), rid);
      return false;
    }

    TRI_shaped_json_t shapedJson;
    TRI_EXTRACT_SHAPED_JSON_MARKER(
        shapedJson, oldDocument.getDataPtr());  // PROTECTED by trx here
    TRI_json_t* old = TRI_JsonShapedJson(shaper, &shapedJson);

    if (old == nullptr) {
      trx.abort();
      generateTransactionError(collectionName, TRI_ERROR_OUT_OF_MEMORY);
      return false;
    }

    std::unique_ptr<TRI_json_t> json(
        arangodb::basics::VelocyPackHelper::velocyPackToJson(body));
    if (ServerState::instance()->isDBServer()) {
      // compare attributes in shardKeys
      if (shardKeysChanged(_request->databaseName(), cidString, old, json.get(),
                           true)) {
        TRI_FreeJson(shaper->memoryZone(), old);

        trx.abort();
        generateTransactionError(
            collectionName,
            TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);

        return false;
      }
    }

    TRI_json_t* patchedJson = TRI_MergeJson(
        TRI_UNKNOWN_MEM_ZONE, old, json.get(), nullMeansRemove, mergeObjects);
    TRI_FreeJson(shaper->memoryZone(), old);

    if (patchedJson == nullptr) {
      trx.abort();
      generateTransactionError(collectionName, TRI_ERROR_OUT_OF_MEMORY);

      return false;
    }

    // do not acquire an extra lock
    res = trx.updateDocument(key, &mptr, patchedJson, policy, waitForSync,
                             revision, &rid);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);
  } else {
    // replacing an existing document, using a lock

    if (ServerState::instance()->isDBServer()) {
      // compare attributes in shardKeys
      // read the existing document
      TRI_doc_mptr_copy_t oldDocument;

      // do not lock again
      trx.lockWrite();

      res = trx.read(&oldDocument, key);
      if (res != TRI_ERROR_NO_ERROR) {
        trx.abort();
        generateTransactionError(collectionName, res,
                                 (TRI_voc_key_t)key.c_str(), rid);
        return false;
      }

      if (oldDocument.getDataPtr() == nullptr) {  // PROTECTED by trx here
        trx.abort();
        generateTransactionError(collectionName,
                                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                 (TRI_voc_key_t)key.c_str(), rid);
        return false;
      }

      TRI_shaped_json_t shapedJson;
      TRI_EXTRACT_SHAPED_JSON_MARKER(
          shapedJson, oldDocument.getDataPtr());  // PROTECTED by trx here
      TRI_json_t* old = TRI_JsonShapedJson(shaper, &shapedJson);

      std::unique_ptr<TRI_json_t> json(
          arangodb::basics::VelocyPackHelper::velocyPackToJson(body));
      if (shardKeysChanged(_request->databaseName(), cidString, old, json.get(),
                           false)) {
        TRI_FreeJson(shaper->memoryZone(), old);

        trx.abort();
        generateTransactionError(
            collectionName,
            TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);

        return false;
      }

      if (old != nullptr) {
        TRI_FreeJson(shaper->memoryZone(), old);
      }
    }

    std::unique_ptr<TRI_json_t> json(
        arangodb::basics::VelocyPackHelper::velocyPackToJson(body));
    res = trx.updateDocument(key, &mptr, json.get(), policy, waitForSync,
                             revision, &rid);
  }

  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, (TRI_voc_key_t)key.c_str(),
                             rid);

    return false;
  }

  generateSaved(trx, cid, mptr);

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
  arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
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
  return responseCode >= arangodb::rest::GeneralResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_DELETE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
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
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid revision number");
    return false;
  }

  // extract or choose the update policy
  TRI_doc_update_policy_e const policy = extractUpdatePolicy();
  bool const waitForSync = extractWaitForSync();

  if (policy == TRI_DOC_UPDATE_ILLEGAL) {
    generateError(GeneralResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "policy must be 'error' or 'last'");
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    return deleteDocumentCoordinator(collection, key, revision, policy,
                                     waitForSync);
  }

  SingleCollectionWriteTransaction<1> trx(new StandaloneTransactionContext(),
                                          _vocbase, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();
  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  std::string collectionName = collection;
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(cid);
  }

  TRI_voc_rid_t rid = 0;
  res = trx.deleteDocument(key, policy, waitForSync, revision, &rid);

  if (res == TRI_ERROR_NO_ERROR) {
    res = trx.commit();
  } else {
    trx.abort();
  }

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, (TRI_voc_key_t)key.c_str(),
                             rid);
    return false;
  }

  generateDeleted(trx, cid, (TRI_voc_key_t)key.c_str(), rid);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocumentCoordinator(
    std::string const& collname, std::string const& key,
    TRI_voc_rid_t const rev, TRI_doc_update_policy_e policy, bool waitForSync) {
  std::string const& dbname = _request->databaseName();
  arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
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
  return responseCode >= arangodb::rest::GeneralResponse::BAD;
}
