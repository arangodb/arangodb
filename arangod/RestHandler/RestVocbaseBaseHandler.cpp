////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Meta/conversion.h"
#include "Rest/CommonDefines.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
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
/// @brief analyzer path
////////////////////////////////////////////////////////////////////////////////
/*static*/ std::string const RestVocbaseBaseHandler::ANALYZER_PATH =
  "/_api/analyzer";

////////////////////////////////////////////////////////////////////////////////
/// @brief batch path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::BATCH_PATH = "/_api/batch";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::COLLECTION_PATH = "/_api/collection";

////////////////////////////////////////////////////////////////////////////////
/// @brief control pregel path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::CONTROL_PREGEL_PATH =
    "/_api/control_pregel";

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::CURSOR_PATH = "/_api/cursor";

////////////////////////////////////////////////////////////////////////////////
/// @brief database path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::DATABASE_PATH = "/_api/database";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::DOCUMENT_PATH = "/_api/document";

////////////////////////////////////////////////////////////////////////////////
/// @brief edges path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EDGES_PATH = "/_api/edges";

////////////////////////////////////////////////////////////////////////////////
/// @brief gharial graph api path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::GHARIAL_PATH = "/_api/gharial";

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::ENDPOINT_PATH = "/_api/endpoint";

////////////////////////////////////////////////////////////////////////////////
/// @brief documents import path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::IMPORT_PATH = "/_api/import";

////////////////////////////////////////////////////////////////////////////////
/// @brief index path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::INDEX_PATH = "/_api/index";

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

//////////////////////////////////////////////////////////////////////////////
/// @brief simple query by example path
//////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE =
    "/_api/simple/by-example";

//////////////////////////////////////////////////////////////////////////////
/// @brief simple query first example path
//////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_FIRST_EXAMPLE =
    "/_api/simple/first-example";

//////////////////////////////////////////////////////////////////////////////
/// @brief simple query remove by example path
//////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_REMOVE_BY_EXAMPLE =
    "/_api/simple/remove-by-example";

//////////////////////////////////////////////////////////////////////////////
/// @brief simple query replace by example path
//////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_REPLACE_BY_EXAMPLE =
    "/_api/simple/replace-by-example";

//////////////////////////////////////////////////////////////////////////////
/// @brief simple query replace by example path
//////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_UPDATE_BY_EXAMPLE =
    "/_api/simple/update-by-example";

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
/// @brief tasks path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::TASKS_PATH = "/_api/tasks";

////////////////////////////////////////////////////////////////////////////////
/// @brief upload path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::UPLOAD_PATH = "/_api/upload";

////////////////////////////////////////////////////////////////////////////////
/// @brief users path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::USERS_PATH = "/_api/user";

////////////////////////////////////////////////////////////////////////////////
/// @brief view path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::VIEW_PATH = "/_api/view";

/// @brief Internal Traverser path

std::string const RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH =
    "/_internal/traverser";

RestVocbaseBaseHandler::RestVocbaseBaseHandler(application_features::ApplicationServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(server, request, response),
      _context(*static_cast<VocbaseContext*>(request->requestContext())),
      _vocbase(_context.vocbase()) {
  TRI_ASSERT(request->requestContext());
}

RestVocbaseBaseHandler::~RestVocbaseBaseHandler() = default;

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestVocbaseBaseHandler::forwardingTarget() {
  bool found = false;
  std::string const& value = _request->header(StaticStrings::TransactionId, found);
  if (found) {
    TransactionId tid = TransactionId::none();
    std::size_t pos = 0;
    try {
      tid = TransactionId{std::stoull(value, &pos, 10)};
    } catch (...) {
    }
    if (tid.isSet()) {
      uint32_t sourceServer = tid.serverId();
      if (sourceServer != ServerState::instance()->getShortId()) {
        auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
        return {std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
      }
    }
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
/// optionally url-encodes
////////////////////////////////////////////////////////////////////////////////

std::string RestVocbaseBaseHandler::assembleDocumentId(std::string const& collectionName,
                                                       std::string const& key,
                                                       bool urlEncode) {
  if (urlEncode) {
    return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + StringUtils::urlEncode(key);
  }
  return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful save
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateSaved(arangodb::OperationResult const& result,
                                           std::string const& collectionName,
                                           TRI_col_type_e type,
                                           VPackOptions const* options, bool isMultiple) {
  generate20x(result, collectionName, type, options, isMultiple, rest::ResponseCode::CREATED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful delete
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDeleted(arangodb::OperationResult const& result,
                                             std::string const& collectionName,
                                             TRI_col_type_e type,
                                             VPackOptions const* options, bool isMultiple) {
  generate20x(result, collectionName, type, options, isMultiple, rest::ResponseCode::OK);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 20x response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x(arangodb::OperationResult const& result,
                                         std::string const& collectionName, TRI_col_type_e type,
                                         VPackOptions const* options, bool isMultiple,
                                         rest::ResponseCode waitForSyncResponseCode) {
  if (result.options.waitForSync) {
    resetResponse(waitForSyncResponseCode);
  } else {
    resetResponse(rest::ResponseCode::ACCEPTED);
  }

  if (isMultiple && !result.countErrorCodes.empty()) {
    VPackBuilder errorBuilder;
    errorBuilder.openObject();
    for (auto const& it : result.countErrorCodes) {
      errorBuilder.add(to_string(it.first), VPackValue(it.second));
    }
    errorBuilder.close();
    _response->setHeaderNC(StaticStrings::ErrorCodes, errorBuilder.toJson());
  }

  VPackSlice slice = result.slice();
  if (slice.isNone()) {
    // will happen if silent == true
    slice = arangodb::velocypack::Slice::emptyObjectSlice();
  } else {
    TRI_ASSERT(slice.isObject() || slice.isArray());
    if (slice.isObject()) {
      _response->setHeaderNC(StaticStrings::Etag,
                             "\"" + slice.get(StaticStrings::RevString).copyString() +
                                 "\"");
      // pre 1.4 location headers withdrawn for >= 3.0
      std::string escapedHandle(
          assembleDocumentId(collectionName,
                             slice.get(StaticStrings::KeyString).copyString(), true));
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

void RestVocbaseBaseHandler::generateConflictError(OperationResult const& opres,
                                                   bool precFailed) {
  TRI_ASSERT(opres.errorNumber() == TRI_ERROR_ARANGO_CONFLICT);
  const auto code = precFailed ? ResponseCode::PRECONDITION_FAILED : ResponseCode::CONFLICT;
  resetResponse(code);
  
  VPackSlice slice = opres.slice();
  if (slice.isObject()) {  // single document case
    std::string const rev =
        VelocyPackHelper::getStringValue(slice, StaticStrings::RevString, "");
    _response->setHeaderNC(StaticStrings::Etag, "\"" + rev + "\"");
  }
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::Error, VPackValue(true));
    builder.add(StaticStrings::Code, VPackValue(static_cast<int32_t>(code)));
    builder.add(StaticStrings::ErrorNum, VPackValue(TRI_ERROR_ARANGO_CONFLICT));
    builder.add(StaticStrings::ErrorMessage, VPackValue(opres.errorMessage()));

    if (slice.isObject()) {
      builder.add(StaticStrings::IdString, slice.get(StaticStrings::IdString));
      builder.add(StaticStrings::KeyString, slice.get(StaticStrings::KeyString));
      builder.add(StaticStrings::RevString, slice.get(StaticStrings::RevString));
    } else {
      builder.add("result", slice);
    }
  }

  auto ctx = transaction::StandaloneContext::Create(_vocbase);

  writeResult(builder.slice(), *(ctx->getVPackOptions()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified(RevisionId rid) {
  resetResponse(rest::ResponseCode::NOT_MODIFIED);
  _response->setHeaderNC(StaticStrings::Etag, "\"" + rid.toString() + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument(VPackSlice const& input, bool generateBody,
                                              VPackOptions const* options) {
  VPackSlice document = input.resolveExternal();

  std::string rev;
  if (document.isObject()) {
    rev =
        VelocyPackHelper::getStringValue(document, StaticStrings::RevString, "");
  }

  // and generate a response
  resetResponse(rest::ResponseCode::OK);

  // set ETAG header
  if (!rev.empty()) {
    _response->setHeaderNC(StaticStrings::Etag, "\"" + rev + "\"");
  }

  try {
    _response->setContentType(_request->contentTypeResponse());
    _response->setGenerateBody(generateBody);
    _response->setPayload(document, *options);
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error, this method is
/// used by the others.
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(std::string const& collectionName,
                                                      OperationResult const& result,
                                                      std::string const& key,
                                                      RevisionId rev) {
  auto const code = result.errorNumber();
  switch (static_cast<int>(code)) {
    case static_cast<int>(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND):
      if (collectionName.empty()) {
        // no collection name specified
        generateError(rest::ResponseCode::BAD, code,
                      "no collection name specified");
      } else {
        // collection name specified but collection not found
        generateError(rest::ResponseCode::NOT_FOUND, code,
                      "collection '" + collectionName + "' not found");
      }
      return;

    case static_cast<int>(TRI_ERROR_ARANGO_READ_ONLY):
      generateError(rest::ResponseCode::FORBIDDEN, code,
                    "collection is read-only");
      return;

    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND):
      generateDocumentNotFound(collectionName, key);
      return;

    case static_cast<int>(TRI_ERROR_ARANGO_CONFLICT):
      if (result.buffer != nullptr && !result.slice().isNone()) {
        // This case happens if we come via the generateTransactionError that
        // has a proper OperationResult with a slice:
        generateConflictError(result, /*precFailed*/ rev.isSet());
      } else {
        // This case happens if we call this method directly with a dummy
        // OperationResult:

        OperationResult tmp(result.result, result.options);
        tmp.buffer = std::make_shared<VPackBufferUInt8>();
        VPackBuilder builder(tmp.buffer);
        builder.openObject();
        builder.add(StaticStrings::IdString,
                    VPackValue(assembleDocumentId(collectionName, key, false)));
        builder.add(StaticStrings::KeyString, VPackValue(key));
        builder.add(StaticStrings::RevString, VPackValue(rev.toString()));
        builder.close();
      
        generateConflictError(tmp, /*precFailed*/ rev.isSet());
      }
      return;

    default:
      generateError(GeneralResponse::responseCode(code), code, result.errorMessage());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

RevisionId RestVocbaseBaseHandler::extractRevision(char const* header, bool& isValid) const {
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

    RevisionId rid = RevisionId::none();

    bool isOld;
    rid = RevisionId::fromString(s, e - s, isOld, false);
    isValid = (rid.isSet() && rid != RevisionId::max());

    return rid;
  }

  return RevisionId::none();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string parameter value
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::extractStringParameter(std::string const& name,
                                                    std::string& ret) const {
  bool found;
  std::string const& value = _request->value(name, found);

  if (found) {
    ret = value;
  }
}

std::unique_ptr<transaction::Methods> RestVocbaseBaseHandler::createTransaction(
    std::string const& collectionName, AccessMode::Type type, OperationOptions const& opOptions) const {
  bool found = false;
  std::string const& value = _request->header(StaticStrings::TransactionId, found);
  if (!found) {
    auto tmp = std::make_unique<SingleCollectionTransaction>(transaction::StandaloneContext::Create(_vocbase),
                                                         collectionName, type);
    if (!opOptions.isSynchronousReplicationFrom.empty() && ServerState::instance()->isDBServer()) {
      tmp->addHint(transaction::Hints::Hint::IS_FOLLOWER_TRX);
    }
    return tmp;
  }
  
  TransactionId tid = TransactionId::none();
  std::size_t pos = 0;
  try {
    tid = TransactionId{std::stoull(value, &pos, 10)};
  } catch (...) {}
  if (!tid.isSet() || (tid.isLegacyTransactionId() &&
                   ServerState::instance()->isRunningInCluster())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid transaction ID");
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (pos > 0 && pos < value.size() &&
      value.compare(pos, std::string::npos, " begin") == 0) {
    if (!ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                     "cannot start a managed transaction here");
    }
    std::string const& trxDef = _request->header(StaticStrings::TransactionBody, found);
    if (found) {
      auto trxOpts = VPackParser::fromJson(trxDef);
      Result res =
          mgr->ensureManagedTrx(_vocbase, tid, trxOpts->slice(),
                                !opOptions.isSynchronousReplicationFrom.empty());
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  }

  auto ctx = mgr->leaseManagedTrx(tid, type);
  if (!ctx) {
    LOG_TOPIC("e94ea", DEBUG, Logger::TRANSACTIONS) << "Transaction with id '" << tid << "' not found";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_NOT_FOUND, std::string("transaction '") + std::to_string(tid.id()) + "' not found");
  }
  return std::make_unique<transaction::Methods>(std::move(ctx));
}

/// @brief create proper transaction context, inclusing the proper IDs
std::shared_ptr<transaction::Context> RestVocbaseBaseHandler::createTransactionContext(AccessMode::Type mode) const {
  bool found = false;
  std::string const& value = _request->header(StaticStrings::TransactionId, found);
  if (!found) {
    return std::make_shared<transaction::StandaloneContext>(_vocbase);
  }

  TransactionId tid = TransactionId::none();
  std::size_t pos = 0;
  try {
    tid = TransactionId{std::stoull(value, &pos, 10)};
  } catch (...) {}
  if (tid.empty() || (tid.isLegacyTransactionId() &&
                      ServerState::instance()->isRunningInCluster())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid transaction ID");
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (pos > 0 && pos < value.size()) {
    if (!tid.isLeaderTransactionId() || !ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                     "illegal to start a managed transaction here");
    }
    if (value.compare(pos, std::string::npos, " aql") == 0) {
      return std::make_shared<transaction::AQLStandaloneContext>(_vocbase, tid);
    } else if (value.compare(pos, std::string::npos, " begin") == 0) {
      // this means we lazily start a transaction
      std::string const& trxDef = _request->header(StaticStrings::TransactionBody, found);
      if (found) {
        auto trxOpts = VPackParser::fromJson(trxDef);
        Result res = mgr->ensureManagedTrx(_vocbase, tid, trxOpts->slice(), false);
        if (res.fail()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }
  }

  auto ctx = mgr->leaseManagedTrx(tid, mode);
  if (!ctx) {
    LOG_TOPIC("2cfed", DEBUG, Logger::TRANSACTIONS) << "Transaction with id '" << tid << "' not found";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_NOT_FOUND,
                                   std::string("transaction '") +
                                       std::to_string(tid.id()) + "' not found");
  }
  return ctx;
}
