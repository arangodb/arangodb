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

#include "RestVocbaseBaseHandler.h"
#include "Basics/conversions.h"
#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "Utils/DocumentHelper.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

#include <velocypack/Builder.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::BATCH_PATH = "/_api/batch";

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::CURSOR_PATH = "/_api/cursor";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::DOCUMENT_PATH = "/_api/document";

////////////////////////////////////////////////////////////////////////////////
/// @brief edge path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EDGE_PATH = "/_api/edge";

////////////////////////////////////////////////////////////////////////////////
/// @brief edges path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EDGES_PATH = "/_api/edges";

////////////////////////////////////////////////////////////////////////////////
/// @brief export path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EXPORT_PATH = "/_api/export";

////////////////////////////////////////////////////////////////////////////////
/// @brief documents import path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::IMPORT_PATH = "/_api/import";

////////////////////////////////////////////////////////////////////////////////
/// @brief replication path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::REPLICATION_PATH =
    "/_api/replication";

////////////////////////////////////////////////////////////////////////////////
/// @brief simple query all path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH =
    "/_api/simple/all";

////////////////////////////////////////////////////////////////////////////////
/// @brief document batch lookup path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH =
    "/_api/simple/lookup-by-keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief document batch remove path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH =
    "/_api/simple/remove-by-keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief upload path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::UPLOAD_PATH = "/_api/upload";

RestVocbaseBaseHandler::RestVocbaseBaseHandler(HttpRequest* request)
    : RestBaseHandler(request),
      _context(static_cast<VocbaseContext*>(request->getRequestContext())),
      _vocbase(_context->getVocbase()),
      _nolockHeaderSet(nullptr) {}

RestVocbaseBaseHandler::~RestVocbaseBaseHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection needs to be created on the fly
///
/// this method will check the "createCollection" attribute of the request. if
/// it is set to true, it will verify that the named collection actually exists.
/// if the collection does not yet exist, it will create it on the fly.
/// if the "createCollection" attribute is not set or set to false, nothing will
/// happen, and the collection name will not be checked
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::checkCreateCollection(std::string const& name,
                                                   TRI_col_type_e type) {
  bool found;
  char const* valueStr = _request->value("createCollection", found);

  if (!found) {
    // "createCollection" parameter not specified
    return true;
  }

  if (!StringUtils::boolean(valueStr)) {
    // "createCollection" parameter specified, but with non-true value
    return true;
  }

  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isDBServer()) {
    // create-collection is not supported in a cluster
    generateTransactionError(name, TRI_ERROR_CLUSTER_UNSUPPORTED);
    return false;
  }

  TRI_vocbase_col_t* collection =
      TRI_FindCollectionByNameOrCreateVocBase(_vocbase, name.c_str(), type);

  if (collection == nullptr) {
    generateTransactionError(name, TRI_errno());
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 201 or 202 response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x(
    HttpResponse::HttpResponseCode responseCode,
    std::string const& collectionName, TRI_voc_key_t key, TRI_voc_rid_t rid,
    TRI_col_type_e type) {
  std::string handle(
      DocumentHelper::assembleDocumentId(collectionName, key));
  std::string rev(StringUtils::itoa(rid));

  createResponse(responseCode);
  _response->setContentType("application/json; charset=utf-8");

  if (responseCode != HttpResponse::OK) {
    // 200 OK is sent is case of delete or update.
    // in these cases we do not return etag nor location
    _response->setHeader("etag", 4, "\"" + rev + "\"");

    std::string escapedHandle(
        DocumentHelper::assembleDocumentId(collectionName, key, true));

    if (_request->compatibility() < 10400L) {
      // pre-1.4 location header (e.g. /_api/document/xyz)
      _response->setHeader("location", 8,
                           std::string(DOCUMENT_PATH + "/" + escapedHandle));
    } else {
      // 1.4+ location header (e.g. /_db/_system/_api/document/xyz)
      if (type == TRI_COL_TYPE_EDGE) {
        _response->setHeader("location", 8,
                             std::string("/_db/" + _request->databaseName() +
                                         EDGE_PATH + "/" + escapedHandle));
      } else {
        _response->setHeader("location", 8,
                             std::string("/_db/" + _request->databaseName() +
                                         DOCUMENT_PATH + "/" + escapedHandle));
      }
    }
  }

  // _id and _key are safe and do not need to be JSON-encoded
  _response->body()
      .appendText("{\"error\":false,\"" TRI_VOC_ATTRIBUTE_ID "\":\"")
      .appendText(handle)
      .appendText("\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"")
      .appendText(rev)
      .appendText("\",\"" TRI_VOC_ATTRIBUTE_KEY "\":\"")
      .appendText(key)
      .appendText("\"}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented(std::string const& path) {
  generateError(HttpResponse::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                "'" + path + "' not implemented");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateForbidden() {
  generateError(HttpResponse::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                "operation forbidden");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed(
    std::string const& collectionName, TRI_voc_key_t key, TRI_voc_rid_t rid) {
  std::string rev(StringUtils::itoa(rid));

  createResponse(HttpResponse::PRECONDITION_FAILED);
  _response->setContentType("application/json; charset=utf-8");
  _response->setHeader("etag", 4, "\"" + rev + "\"");

  // _id and _key are safe and do not need to be JSON-encoded
  _response->body()
      .appendText("{\"error\":true,\"code\":")
      .appendInteger((int32_t)HttpResponse::PRECONDITION_FAILED)
      .appendText(",\"errorNum\":")
      .appendInteger((int32_t)TRI_ERROR_ARANGO_CONFLICT)
      .appendText(",\"errorMessage\":\"precondition failed\"")
      .appendText(",\"" TRI_VOC_ATTRIBUTE_ID "\":\"")
      .appendText(DocumentHelper::assembleDocumentId(collectionName, key))
      .appendText("\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"")
      .appendText(StringUtils::itoa(rid))
      .appendText("\",\"" TRI_VOC_ATTRIBUTE_KEY "\":\"")
      .appendText(key)
      .appendText("\"}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified(TRI_voc_rid_t rid) {
  std::string const&& rev = StringUtils::itoa(rid);

  createResponse(HttpResponse::NOT_MODIFIED);
  _response->setHeader("etag", 4, "\"" + rev + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument(
    SingleCollectionReadOnlyTransaction& trx, TRI_voc_cid_t cid,
    TRI_doc_mptr_copy_t const& mptr, VocShaper* shaper, bool generateBody) {
  CollectionNameResolver const* resolver = trx.resolver();

  char const* key =
      TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx from above
  std::string id(DocumentHelper::assembleDocumentId(
      resolver->getCollectionName(cid), key));

  try {
    VPackBuilder builder;
    builder.openObject();

    builder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(id));

    // convert rid from uint64_t to string
    std::string rev(StringUtils::itoa(mptr._rid));
    builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(rev));

    builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));

    TRI_df_marker_type_t type =
        static_cast<TRI_df_marker_t const*>(mptr.getDataPtr())
            ->_type;  // PROTECTED by trx passed from above

    if (type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* marker =
          static_cast<TRI_doc_edge_key_marker_t const*>(
              mptr.getDataPtr());  // PROTECTED by trx passed from above
      std::string from(DocumentHelper::assembleDocumentId(
          resolver->getCollectionNameCluster(marker->_fromCid),
          std::string((char*)marker + marker->_offsetFromKey)));
      builder.add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(from));

      std::string to(DocumentHelper::assembleDocumentId(
          resolver->getCollectionNameCluster(marker->_toCid),
          std::string((char*)marker + marker->_offsetToKey)));
      builder.add(TRI_VOC_ATTRIBUTE_TO, VPackValue(to));
    } else if (type == TRI_WAL_MARKER_EDGE) {
      arangodb::wal::edge_marker_t const* marker =
          static_cast<arangodb::wal::edge_marker_t const*>(
              mptr.getDataPtr());  // PROTECTED by trx passed from above
      std::string from(DocumentHelper::assembleDocumentId(
          resolver->getCollectionNameCluster(marker->_fromCid),
          std::string((char*)marker + marker->_offsetFromKey)));
      builder.add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(from));

      std::string to(DocumentHelper::assembleDocumentId(
          resolver->getCollectionNameCluster(marker->_toCid),
          std::string((char*)marker + marker->_offsetToKey)));
      builder.add(TRI_VOC_ATTRIBUTE_TO, VPackValue(to));
    }
    builder.close();

    // add document identifier to buffer
    TRI_string_buffer_t buffer;

    // convert object to string
    TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

    TRI_shaped_json_t shapedJson;
    TRI_EXTRACT_SHAPED_JSON_MARKER(
        shapedJson, mptr.getDataPtr());  // PROTECTED by trx passed from above

    std::unique_ptr<TRI_json_t> augmented(
        arangodb::basics::VelocyPackHelper::velocyPackToJson(builder.slice()));
    TRI_StringifyAugmentedShapedJson(shaper, &buffer, &shapedJson,
                                     augmented.get());

    // and generate a response
    createResponse(HttpResponse::OK);
    _response->setContentType("application/json; charset=utf-8");
    _response->setHeader("etag", 4, "\"" + rev + "\"");

    if (generateBody) {
      _response->body().appendText(TRI_BeginStringBuffer(&buffer),
                                   TRI_LengthStringBuffer(&buffer));
    } else {
      _response->headResponse(TRI_LengthStringBuffer(&buffer));
    }

    TRI_DestroyStringBuffer(&buffer);

  } catch (arangodb::velocypack::Exception const&) {
    // TODO What should happen on error here?
    // Failed to build the object
    // All other Exceptions were not catched before
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(
    std::string const& collectionName, int res, TRI_voc_key_t key,
    TRI_voc_rid_t rid) {
  switch (res) {
    case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
      if (collectionName.empty()) {
        // no collection name specified
        generateError(HttpResponse::BAD, res, "no collection name specified");
      } else {
        // collection name specified but collection not found
        generateError(HttpResponse::NOT_FOUND, res,
                      "collection '" + collectionName + "' not found");
      }
      return;

    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(HttpResponse::FORBIDDEN, res, "collection is read-only");
      return;
    
    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(HttpResponse::CONFLICT, res,
                    "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(HttpResponse::BAD, res, "invalid document key");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(HttpResponse::SERVER_ERROR, res, "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(HttpResponse::BAD, res,
                    "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateDocumentNotFound(collectionName, key);
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
      generateError(HttpResponse::BAD, res);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(collectionName,
                                 key ? key : (TRI_voc_key_t) "unknown", rid);
      return;

    case TRI_ERROR_CLUSTER_SHARD_GONE:
      generateError(HttpResponse::SERVER_ERROR, res,
                    "coordinator: no responsible shard found");
      return;

    case TRI_ERROR_CLUSTER_TIMEOUT:
      generateError(HttpResponse::SERVER_ERROR, res);
      return;

    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY: {
      generateError(HttpResponse::BAD, res);
      return;
    }

    case TRI_ERROR_CLUSTER_UNSUPPORTED: {
      generateError(HttpResponse::NOT_IMPLEMENTED, res);
      return;
    }
    
    case TRI_ERROR_FORBIDDEN: {
      generateError(HttpResponse::FORBIDDEN, res);
      return;
    }
        
    case TRI_ERROR_OUT_OF_MEMORY: 
    case TRI_ERROR_LOCK_TIMEOUT: 
    case TRI_ERROR_AID_NOT_FOUND:
    case TRI_ERROR_DEBUG:
    case TRI_ERROR_LEGEND_NOT_IN_WAL_FILE:
    case TRI_ERROR_LOCKED:
    case TRI_ERROR_DEADLOCK: {
      generateError(HttpResponse::SERVER_ERROR, res);
      return;
    }

    default:
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "failed with error: " + std::string(TRI_errno_string(res)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t RestVocbaseBaseHandler::extractRevision(char const* header,
                                                      char const* parameter,
                                                      bool& isValid) {
  isValid = true;
  bool found;
  char const* etag = _request->header(header, found);

  if (found) {
    char const* s = etag;
    char const* e = etag + strlen(etag);

    while (s < e && (s[0] == ' ' || s[0] == '\t')) {
      ++s;
    }

    if (s < e && (s[0] == '"' || s[0] == '\'')) {
      ++s;
    }

    while (s < e && (e[-1] == ' ' || e[-1] == '\t')) {
      --e;
    }

    if (s < e && (e[-1] == '"' || e[-1] == '\'')) {
      --e;
    }

    TRI_voc_rid_t rid = TRI_UInt64String2(s, e - s);
    isValid = (TRI_errno() != TRI_ERROR_ILLEGAL_NUMBER);

    return rid;
  }

  if (parameter != nullptr) {
    etag = _request->value(parameter, found);

    if (found) {
      TRI_voc_rid_t rid = TRI_UInt64String(etag);
      isValid = (TRI_errno() != TRI_ERROR_ILLEGAL_NUMBER);
      return rid;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the update policy
////////////////////////////////////////////////////////////////////////////////

TRI_doc_update_policy_e RestVocbaseBaseHandler::extractUpdatePolicy() const {
  bool found;
  char const* policy = _request->value("policy", found);

  if (found) {
    if (TRI_CaseEqualString(policy, "error")) {
      return TRI_DOC_UPDATE_ERROR;
    } else if (TRI_CaseEqualString(policy, "last")) {
      return TRI_DOC_UPDATE_LAST_WRITE;
    } else {
      return TRI_DOC_UPDATE_ILLEGAL;
    }
  } else {
    return TRI_DOC_UPDATE_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the waitForSync value
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::extractWaitForSync() const {
  bool found;
  char const* forceStr = _request->value("waitForSync", found);

  if (found) {
    return StringUtils::boolean(forceStr);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body as VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestVocbaseBaseHandler::parseVelocyPackBody(
    VPackOptions const* options, bool& success) {
  try {
    success = true;
    return _request->toVelocyPack(options);
  } catch (std::bad_alloc const&) {
    generateOOMError();
  } catch (VPackException const& e) {
    std::string errmsg("Parse error: ");
    errmsg.append(e.what());
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON, errmsg);
  }
  success = false;
  VPackParser p;
  return p.steal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle
/// TODO: merge with DocumentHelper::parseDocumentId
////////////////////////////////////////////////////////////////////////////////

int RestVocbaseBaseHandler::parseDocumentId(
    CollectionNameResolver const* resolver, std::string const& handle,
    TRI_voc_cid_t& cid, TRI_voc_key_t& key) {
  char const* ptr = handle.c_str();
  char const* end = ptr + handle.size();

  if (end - ptr < 3) {
    // minimum length of document id is 3:
    // at least 1 byte for collection name, '/' + at least 1 byte for key
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  char const* pos = static_cast<char const*>(
      memchr(static_cast<void const*>(ptr), TRI_DOCUMENT_HANDLE_SEPARATOR_CHR,
             handle.size()));

  if (pos == nullptr || pos >= end - 1) {
    // if no '/' is found, the id is invalid
    // if '/' is at the very end, the id is invalid too
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  // check if the id contains a second '/'
  if (memchr(static_cast<void const*>(pos + 1),
             TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, end - pos - 1) != nullptr) {
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  char const first = *ptr;

  if (first >= '0' && first <= '9') {
    cid = TRI_UInt64String2(ptr, ptr - pos);
  } else {
    cid = resolver->getCollectionIdCluster(std::string(ptr, pos - ptr));
  }

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  key = TRI_DuplicateString(TRI_CORE_MEM_ZONE, pos + 1, end - pos - 1);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepareExecute, to react to X-Arango-Nolock header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::prepareExecute() {
  RestBaseHandler::prepareExecute();

  bool found;
  char const* shardId = _request->header("x-arango-nolock", found);

  if (found) {
    _nolockHeaderSet = new std::unordered_set<std::string>();
    _nolockHeaderSet->insert(std::string(shardId));
    arangodb::Transaction::_makeNolockHeaders = _nolockHeaderSet;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalizeExecute, to react to X-Arango-Nolock header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::finalizeExecute() {
  if (_nolockHeaderSet != nullptr) {
    arangodb::Transaction::_makeNolockHeaders = nullptr;
    delete _nolockHeaderSet;
    _nolockHeaderSet = nullptr;
  }

  RestBaseHandler::finalizeExecute();
}
