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
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Meta/conversion.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief agency public path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::AGENCY_PATH = "/_api/agency";

////////////////////////////////////////////////////////////////////////////////
/// @brief agency private path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::AGENCY_PRIV_PATH =
    "/_api/agency_priv";

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
/// @brief simple query all-keys path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH =
    "/_api/simple/all-keys";

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

////////////////////////////////////////////////////////////////////////////////
/// @brief wal path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::WAL_PATH = "/_admin/wal";

/// @brief Internal Traverser path

std::string const RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH = "/_internal/traverser";

RestVocbaseBaseHandler::RestVocbaseBaseHandler(GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(request, response),
      _context(static_cast<VocbaseContext*>(request->requestContext())),
      _vocbase(_context->vocbase()),
      _nolockHeaderSet(nullptr) {}

RestVocbaseBaseHandler::~RestVocbaseBaseHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
/// optionally url-encodes
////////////////////////////////////////////////////////////////////////////////

std::string RestVocbaseBaseHandler::assembleDocumentId(
    std::string const& collectionName, std::string const& key, bool urlEncode) {
  if (urlEncode) {
    return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR +
           StringUtils::urlEncode(key);
  }
  return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful save
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateSaved(
    arangodb::OperationResult const& result, std::string const& collectionName,
    TRI_col_type_e type, VPackOptions const* options, bool isMultiple) {
  if (result.wasSynchronous) {
    resetResponse(rest::ResponseCode::CREATED);
  } else {
    resetResponse(rest::ResponseCode::ACCEPTED);
  }

  if (isMultiple && !result.countErrorCodes.empty()) {
    VPackBuilder errorBuilder;
    errorBuilder.openObject();
    for (auto const& it : result.countErrorCodes) {
      errorBuilder.add(basics::StringUtils::itoa(it.first),
                       VPackValue(it.second));
    }
    errorBuilder.close();
    _response->setHeaderNC(StaticStrings::ErrorCodes, errorBuilder.toJson());
  }

  generate20x(result, collectionName, type, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful delete
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDeleted(
    arangodb::OperationResult const& result, std::string const& collectionName,
    TRI_col_type_e type, VPackOptions const* options) {
  if (result.wasSynchronous) {
    resetResponse(rest::ResponseCode::OK);
  } else {
    resetResponse(rest::ResponseCode::ACCEPTED);
  }
  generate20x(result, collectionName, type, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 20x response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x(
    arangodb::OperationResult const& result, std::string const& collectionName,
    TRI_col_type_e type, VPackOptions const* options) {
  VPackSlice slice = result.slice();
  if (slice.isNone()) {
    // will happen if silent == true
    slice = VelocyPackHelper::EmptyObjectValue();
  } else {
    TRI_ASSERT(slice.isObject() || slice.isArray());
    if (slice.isObject()) {
      _response->setHeaderNC(
          StaticStrings::Etag,
          "\"" + slice.get(StaticStrings::RevString).copyString() + "\"");
      // pre 1.4 location headers withdrawn for >= 3.0
      std::string escapedHandle(assembleDocumentId(
          collectionName, slice.get(StaticStrings::KeyString).copyString(),
          true));
      _response->setHeaderNC(StaticStrings::Location,
                             std::string("/_db/" + _request->databaseName() +
                                         DOCUMENT_PATH + "/" + escapedHandle));
    }
  }

  writeResult(slice, *options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented(std::string const& path) {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                "'" + path + "' not implemented");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateForbidden() {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                "operation forbidden");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed(
    VPackSlice const& slice) {
  resetResponse(rest::ResponseCode::PRECONDITION_FAILED);

  if (slice.isObject()) {  // single document case
    std::string const rev =
        VelocyPackHelper::getStringValue(slice, StaticStrings::RevString, "");
    _response->setHeaderNC(StaticStrings::Etag, "\"" + rev + "\"");
  }
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add("error", VPackValue(true));
    builder.add("code", VPackValue(static_cast<int32_t>(
                            rest::ResponseCode::PRECONDITION_FAILED)));
    builder.add("errorNum", VPackValue(TRI_ERROR_ARANGO_CONFLICT));
    builder.add("errorMessage", VPackValue("precondition failed"));
    if (slice.isObject()) {
      builder.add(StaticStrings::IdString, slice.get(StaticStrings::IdString));
      builder.add(StaticStrings::KeyString,
                  slice.get(StaticStrings::KeyString));
      builder.add(StaticStrings::RevString,
                  slice.get(StaticStrings::RevString));
    } else {
      builder.add("result", slice);
    }
  }

  auto transactionContext(StandaloneTransactionContext::Create(_vocbase));
  writeResult(builder.slice(), *(transactionContext->getVPackOptionsForDump()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed(
    std::string const& collectionName, std::string const& key,
    TRI_voc_rid_t rev) {
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::IdString,
              VPackValue(assembleDocumentId(collectionName, key, false)));
  builder.add(StaticStrings::KeyString, VPackValue(key));
  builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(rev)));
  builder.close();

  generatePreconditionFailed(builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified(TRI_voc_rid_t rid) {
  resetResponse(rest::ResponseCode::NOT_MODIFIED);
  _response->setHeaderNC(StaticStrings::Etag,
                         "\"" + TRI_RidToString(rid) + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument(VPackSlice const& input,
                                              bool generateBody,
                                              VPackOptions const* options) {
  VPackSlice document = input.resolveExternal();

  std::string rev;
  if (document.isObject()) {
    rev = VelocyPackHelper::getStringValue(document, StaticStrings::RevString,
                                           "");
  }

  // and generate a response
  resetResponse(rest::ResponseCode::OK);

  // set ETAG header
  if (!rev.empty()) {
    _response->setHeaderNC(StaticStrings::Etag, "\"" + rev + "\"");
  }

  try {
    _response->setContentType(_request->contentTypeResponse());
    _response->setPayload(document, generateBody, *options);
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(
    std::string const& collectionName, int res, std::string const& key,
    TRI_voc_rid_t rev) {
  switch (res) {
    case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
      if (collectionName.empty()) {
        // no collection name specified
        generateError(rest::ResponseCode::BAD, res,
                      "no collection name specified");
      } else {
        // collection name specified but collection not found
        generateError(rest::ResponseCode::NOT_FOUND, res,
                      "collection '" + collectionName + "' not found");
      }
      return;

    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(rest::ResponseCode::FORBIDDEN, res,
                    "collection is read-only");
      return;

    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(rest::ResponseCode::CONFLICT, res,
                    "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(rest::ResponseCode::BAD, res, "invalid document key");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD:
      generateError(rest::ResponseCode::BAD, res, "invalid document handle");
      return;

    case TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
      generateError(rest::ResponseCode::BAD, res, "invalid edge attribute");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(rest::ResponseCode::SERVER_ERROR, res, "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(rest::ResponseCode::BAD, res,
                    "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateDocumentNotFound(collectionName, key);
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
      generateError(rest::ResponseCode::BAD, res);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(collectionName, key.empty() ? "unknown" : key,
                                 rev);
      return;

    case TRI_ERROR_CLUSTER_SHARD_GONE:
      generateError(rest::ResponseCode::SERVER_ERROR, res,
                    "coordinator: no responsible shard found");
      return;

    case TRI_ERROR_CLUSTER_TIMEOUT:
      generateError(rest::ResponseCode::SERVER_ERROR, res);
      return;

    case TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE:
      generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, res,
                    "A required backend was not available");
      return;

    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY: {
      generateError(rest::ResponseCode::BAD, res);
      return;
    }

    case TRI_ERROR_CLUSTER_UNSUPPORTED: {
      generateError(rest::ResponseCode::NOT_IMPLEMENTED, res);
      return;
    }

    case TRI_ERROR_FORBIDDEN: {
      generateError(rest::ResponseCode::FORBIDDEN, res);
      return;
    }

    case TRI_ERROR_OUT_OF_MEMORY:
    case TRI_ERROR_LOCK_TIMEOUT:
    case TRI_ERROR_DEBUG:
    case TRI_ERROR_LOCKED:
    case TRI_ERROR_DEADLOCK: {
      generateError(rest::ResponseCode::SERVER_ERROR, res);
      return;
    }

    default:
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "failed with error: " + std::string(TRI_errno_string(res)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(
    OperationResult const& result) {
  switch (result.code) {
    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(rest::ResponseCode::FORBIDDEN, result.code,
                    "collection is read-only");
      return;

    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(rest::ResponseCode::CONFLICT, result.code,
                    "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(rest::ResponseCode::BAD, result.code,
                    "invalid document key");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD:
      generateError(rest::ResponseCode::BAD, result.code,
                    "invalid document handle");
      return;

    case TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
      generateError(rest::ResponseCode::BAD, result.code,
                    "invalid edge attribute");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(rest::ResponseCode::SERVER_ERROR, result.code,
                    "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(rest::ResponseCode::BAD, result.code,
                    "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
      generateError(rest::ResponseCode::BAD, result.code);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(result.slice());
      return;

    case TRI_ERROR_CLUSTER_SHARD_GONE:
      generateError(rest::ResponseCode::SERVER_ERROR, result.code,
                    "coordinator: no responsible shard found");
      return;

    case TRI_ERROR_CLUSTER_TIMEOUT:
      generateError(rest::ResponseCode::SERVER_ERROR, result.code);
      return;

    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY: {
      generateError(rest::ResponseCode::BAD, result.code);
      return;
    }

    case TRI_ERROR_CLUSTER_UNSUPPORTED: {
      generateError(rest::ResponseCode::NOT_IMPLEMENTED, result.code);
      return;
    }

    case TRI_ERROR_FORBIDDEN: {
      generateError(rest::ResponseCode::FORBIDDEN, result.code);
      return;
    }

    case TRI_ERROR_OUT_OF_MEMORY:
    case TRI_ERROR_LOCK_TIMEOUT:
    case TRI_ERROR_DEBUG:
    case TRI_ERROR_LOCKED:
    case TRI_ERROR_DEADLOCK: {
      generateError(rest::ResponseCode::SERVER_ERROR, result.code);
      return;
    }

    default:
      generateError(
          rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
          "failed with error: " + std::string(TRI_errno_string(result.code)));
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
  std::string const& etag = _request->header(header, found);

  if (found) {
    char const* s = etag.c_str();
    char const* e = s + etag.size();

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

    TRI_voc_rid_t rid = 0;

    bool isOld;
    rid = TRI_StringToRidWithCheck(s, e - s, isOld);
    isValid = (rid != 0);

    return rid;
  }

  if (parameter != nullptr) {
    std::string const& etag2 = _request->value(parameter, found);

    if (found) {
      TRI_voc_rid_t rid = 0;

      bool isOld;
      rid = TRI_StringToRidWithCheck(etag2, isOld);
      isValid = (rid != 0);

      return rid;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a boolean parameter value
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::extractBooleanParameter(char const* name,
                                                     bool def) const {
  bool found;
  std::string const& value = _request->value(name, found);

  if (found) {
    return StringUtils::boolean(value);
  }

  return def;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepareExecute, to react to X-Arango-Nolock header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::prepareExecute() {
  RestBaseHandler::prepareExecute();

  bool found;
  std::string const& shardId = _request->header("x-arango-nolock", found);

  if (found) {
    _nolockHeaderSet =
        new std::unordered_set<std::string>{std::string(shardId)};
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
