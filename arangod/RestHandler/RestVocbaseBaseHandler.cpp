////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogContextKeys.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Logger/LogStructuredParamsAllowList.h"
#include "Meta/conversion.h"
#include "Rest/CommonDefines.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

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
/// @brief collection path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::COLLECTION_PATH = "/_api/collection";

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

RestVocbaseBaseHandler::RestVocbaseBaseHandler(ArangodServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(server, request, response),
      _context(static_cast<VocbaseContext&>(*request->requestContext())),
      _vocbase(_context.vocbase()),
      _scopeVocbaseValues(
          LogContext::makeValue()
              .with<structuredParams::DatabaseName>(_vocbase.name())
              .share()) {
  TRI_ASSERT(request->requestContext());
}

void RestVocbaseBaseHandler::prepareExecute(bool isContinue) {
  RestHandler::prepareExecute(isContinue);
  _logContextVocbaseEntry =
      LogContext::Current::pushValues(_scopeVocbaseValues);
}

void RestVocbaseBaseHandler::shutdownExecute(bool isFinalized) noexcept {
  LogContext::Current::popEntry(_logContextVocbaseEntry);
  RestHandler::shutdownExecute(isFinalized);
}

RestVocbaseBaseHandler::~RestVocbaseBaseHandler() = default;

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>>
RestVocbaseBaseHandler::forwardingTarget() {
  bool found = false;
  std::string const& value =
      _request->header(StaticStrings::TransactionId, found);
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
        return {
            std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
      }
    }
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
/// optionally url-encodes
////////////////////////////////////////////////////////////////////////////////

std::string RestVocbaseBaseHandler::assembleDocumentId(
    std::string_view collectionName, std::string_view key,
    bool urlEncode) const {
  if (urlEncode) {
    return absl::StrCat(collectionName, TRI_DOCUMENT_HANDLE_SEPARATOR_STR,
                        StringUtils::urlEncode(key));
  }
  return absl::StrCat(collectionName, TRI_DOCUMENT_HANDLE_SEPARATOR_STR, key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 20x response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x(
    OperationResult const& result, std::string_view collectionName,
    TRI_col_type_e type, velocypack::Options const* options, bool isMultiple,
    bool silent, rest::ResponseCode waitForSyncResponseCode) {
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
  if (slice.isNone() || (!isMultiple && silent && result.ok())) {
    // slice::none case will happen if silent == true on single server.
    // in the cluster (coordinator case), we may get the actual operation
    // result here for _single_ operations even if silent === true.
    // thus we fake the result here to be empty again, so that no data
    // is returned if silent === true.
    slice = velocypack::Slice::emptyObjectSlice();
  } else {
    TRI_ASSERT(slice.isObject() || slice.isArray());
    if (slice.isObject()) {
      _response->setHeaderNC(
          StaticStrings::Etag,
          absl::StrCat("\"", slice.get(StaticStrings::RevString).stringView(),
                       "\""));
      // pre 1.4 location headers withdrawn for >= 3.0
      std::string escapedHandle(assembleDocumentId(
          collectionName, slice.get(StaticStrings::KeyString).stringView(),
          true));
      _response->setHeaderNC(
          StaticStrings::Location,
          absl::StrCat("/_db/",
                       StringUtils::urlEncode(_request->databaseName()),
                       DOCUMENT_PATH, "/", escapedHandle));
    }
  }

  writeResult(slice, *options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateConflictError(OperationResult const& opres,
                                                   bool precFailed) {
  TRI_ASSERT(opres.is(TRI_ERROR_ARANGO_CONFLICT));
  auto code =
      precFailed ? ResponseCode::PRECONDITION_FAILED : ResponseCode::CONFLICT;
  resetResponse(code);

  VPackSlice slice = opres.slice();
  if (slice.isObject()) {  // single document case
    if (auto rev = slice.get(StaticStrings::RevString); rev.isString()) {
      // we need to check whether we actually have a revision id here.
      // this method is called not only for returned documents, but also
      // from code locations that do not return documents!
      _response->setHeaderNC(StaticStrings::Etag,
                             absl::StrCat("\"", rev.stringView(), "\""));
    }
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
      builder.add(StaticStrings::KeyString,
                  slice.get(StaticStrings::KeyString));
      builder.add(StaticStrings::RevString,
                  slice.get(StaticStrings::RevString));
    } else {
      builder.add("result", slice);
    }
  }

  auto origin = transaction::OperationOriginInternal{"writing result"};
  auto ctx = transaction::StandaloneContext::create(_vocbase, origin);
  writeResult(builder.slice(), *(ctx->getVPackOptions()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified(RevisionId rid) {
  resetResponse(rest::ResponseCode::NOT_MODIFIED);
  _response->setHeaderNC(StaticStrings::Etag,
                         absl::StrCat("\"", rid.toString(), "\""));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument(
    velocypack::Slice input, bool generateBody,
    velocypack::Options const* options) {
  // and generate a response
  resetResponse(rest::ResponseCode::OK);

  VPackSlice document = input.resolveExternal();
  // set ETAG header
  if (document.isObject()) {
    // we need to check whether we actually have a revision id here.
    // this method is called not only for returned documents, but also
    // from code locations that do not return documents!
    if (auto rev = document.get(StaticStrings::RevString); rev.isString()) {
      _response->setHeaderNC(StaticStrings::Etag,
                             absl::StrCat("\"", rev.stringView(), "\""));
    }
  }
  if (_potentialDirtyReads) {
    _response->setHeaderNC(StaticStrings::PotentialDirtyRead, "true");
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

void RestVocbaseBaseHandler::generateTransactionError(
    std::string_view collectionName, OperationResult const& result,
    std::string_view key, RevisionId rev) {
  auto const code = result.errorNumber();
  switch (static_cast<int>(code)) {
    case static_cast<int>(
        TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_AVAILABLE): {
      // Can only show up in Replication2. But uncritical if ever changed.
      TRI_ASSERT(_vocbase.replicationVersion() == replication::Version::TWO);
      auto const& res = result.result;
      auto const& opOptions = result.options;
      // The Not_Acceptable response is just for compatibility.
      // In Replication2 we do never send the isSynchronousReplication header.
      auto respCode = opOptions.isSynchronousReplicationFrom.empty()
                          ? ResponseCode::MISDIRECTED_REQUEST
                          : ResponseCode::NOT_ACCEPTABLE;
      generateError(respCode, res.errorNumber(), res.errorMessage());
      return;
    }
    case static_cast<int>(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND):
      if (collectionName.empty()) {
        // no collection name specified
        generateError(rest::ResponseCode::BAD, code,
                      "no collection name specified");
      } else {
        // collection name specified but collection not found
        generateError(
            rest::ResponseCode::NOT_FOUND, code,
            absl::StrCat("collection '", collectionName, "' not found"));
      }
      return;

    case static_cast<int>(TRI_ERROR_ARANGO_READ_ONLY):
      generateError(rest::ResponseCode::FORBIDDEN, code,
                    "collection is read-only");
      return;

    case static_cast<int>(TRI_ERROR_REPLICATION_WRITE_CONCERN_NOT_FULFILLED):
      // This deserves an explanation: If the write concern for a write
      // operation is not fulfilled, then we do not want to retry
      // cluster-internally, or else the client would only be informed
      // about the problem once a longish timeout of 15 minutes has passed.
      // Rather, we want to report back the problem immediately. Therefore,
      // a coordinator will return 503 as it should be in this case, after
      // all the request is retryable. But a dbserver must return 403, such
      // that the coordinator who initiated the request will *not* retry.
      if (ServerState::instance()->isCoordinator()) {
        generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, code,
                      "write concern not fulfilled");
      } else {
        generateError(rest::ResponseCode::FORBIDDEN, code,
                      "write concern not fulfilled");
      }
      return;
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND):
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
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
      generateError(GeneralResponse::responseCode(code), code,
                    result.errorMessage());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

RevisionId RestVocbaseBaseHandler::extractRevision(char const* header,
                                                   bool& isValid) const {
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

    RevisionId rid = RevisionId::fromString({s, static_cast<size_t>(e - s)});
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

futures::Future<std::unique_ptr<transaction::Methods>>
RestVocbaseBaseHandler::createTransaction(
    std::string const& collectionName, AccessMode::Type type,
    OperationOptions const& opOptions,
    transaction::OperationOrigin operationOrigin,
    transaction::Options&& trxOpts) const {
  bool const isDBServer = ServerState::instance()->isDBServer();
  bool isFollower =
      !opOptions.isSynchronousReplicationFrom.empty() && isDBServer;

  bool found = false;
  std::string const& value =
      _request->header(StaticStrings::TransactionId, found);
  if (!found) {
    if (opOptions.allowDirtyReads && AccessMode::isRead(type)) {
      trxOpts.allowDirtyReads = true;
    }
    auto tmp = std::make_unique<SingleCollectionTransaction>(
        transaction::StandaloneContext::create(_vocbase, operationOrigin),
        collectionName, type, std::move(trxOpts));
    if (isFollower) {
      tmp->addHint(transaction::Hints::Hint::IS_FOLLOWER_TRX);
    }
    if (isDBServer) {
      // set username from request
      tmp->setUsername(_request->value(StaticStrings::UserString));
    }
    co_return tmp;
  }

  TransactionId tid = TransactionId::none();
  std::size_t pos = 0;
  try {
    tid = TransactionId{std::stoull(value, &pos, 10)};
  } catch (...) {
  }
  if (!tid.isSet() || (tid.isLegacyTransactionId() &&
                       ServerState::instance()->isRunningInCluster())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid transaction ID");
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (pos > 0 && pos < value.size()) {
    if (value.compare(pos, std::string::npos, " aql") == 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                     "cannot start an AQL transaction here");
    }

    if (value.compare(pos, std::string::npos, " begin") == 0) {
      if (!ServerState::instance()->isDBServer()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
            "cannot start a managed transaction here");
      }
      std::string const& trxDef =
          _request->header(StaticStrings::TransactionBody, found);
      if (found) {
        auto trxOpts = VPackParser::fromJson(trxDef);
        Result res = co_await mgr->ensureManagedTrx(
            _vocbase, tid, trxOpts->slice(), operationOrigin, isFollower);
        if (res.fail()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }
  }

  // if we have a read operation and the x-arango-aql-document-call header is
  // set, it means this is a request by the DOCUMENT function inside an AQL
  // query. in this case, we cannot be sure to lease the transaction context
  // successfully, because the AQL query may have already acquired the write
  // lock on the context for the entire duration of the query. if this is the
  // case, then the query already has the lock, and it is ok if we lease the
  // context here without acquiring it again.
  bool isSideUser = (isDBServer && AccessMode::isRead(type) &&
                     !_request->header(StaticStrings::AqlDocumentCall).empty());

  std::shared_ptr<transaction::Context> ctx =
      co_await mgr->leaseManagedTrx(tid, type, isSideUser);

  if (!ctx) {
    LOG_TOPIC("e94ea", DEBUG, Logger::TRANSACTIONS)
        << "Transaction with id " << tid << " not found";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_NOT_FOUND,
        absl::StrCat("transaction ", tid.id(), " not found"));
  }

  std::unique_ptr<transaction::Methods> trx;
  if (isFollower) {
    // a write request from synchronous replication
    TRI_ASSERT(AccessMode::isWriteOrExclusive(type));
    // inject at least the required collection name
    trx = std::make_unique<transaction::Methods>(std::move(ctx), collectionName,
                                                 type);
  } else {
    if (isSideUser) {
      // this is a call from the DOCUMENT() AQL function into an existing AQL
      // query. locks are already acquired by the AQL transaction.
      if (type != AccessMode::Type::READ) {
        basics::abortOrThrow(TRI_ERROR_INTERNAL,
                             "invalid access mode for request", ADB_HERE);
      }
    }
    trx = std::make_unique<transaction::Methods>(std::move(ctx));
  }
  if (isDBServer) {
    // set username from request
    trx->setUsername(_request->value(StaticStrings::UserString));
  }
  co_return trx;
}

/// @brief create proper transaction context, including the proper IDs
futures::Future<std::shared_ptr<transaction::Context>>
RestVocbaseBaseHandler::createTransactionContext(
    AccessMode::Type mode, transaction::OperationOrigin operationOrigin) const {
  bool found = false;
  std::string const& value =
      _request->header(StaticStrings::TransactionId, found);
  if (!found) {
    co_return std::make_shared<transaction::StandaloneContext>(_vocbase,
                                                               operationOrigin);
  }

  TransactionId tid = TransactionId::none();
  std::size_t pos = 0;
  try {
    tid = TransactionId{std::stoull(value, &pos, 10)};
  } catch (...) {
  }
  if (tid.empty() || (tid.isLegacyTransactionId() &&
                      ServerState::instance()->isRunningInCluster())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid transaction ID");
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (pos > 0 && pos < value.size()) {
    if (!tid.isLeaderTransactionId() ||
        !ServerState::instance()->isDBServer()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
          "illegal to start a managed transaction here");
    }
    if (value.compare(pos, std::string::npos, " aql") == 0) {
      // standalone AQL query on DB server
      auto ctx = std::make_shared<transaction::AQLStandaloneContext>(
          _vocbase, tid, operationOrigin);
      co_return ctx;
    }

    if (value.compare(pos, std::string::npos, " begin") == 0) {
      // this means we lazily start a transaction
      std::string const& trxDef =
          _request->header(StaticStrings::TransactionBody, found);
      if (found) {
        auto trxOpts = VPackParser::fromJson(trxDef);
        // this is the first time we see this transaction on this
        // DB server. override origin, because we are inside a streaming
        // transaction when we get here.
        auto origin = transaction::OperationOriginREST{
            "streaming transaction on DB server"};
        Result res = co_await mgr->ensureManagedTrx(
            _vocbase, tid, trxOpts->slice(), origin, false);
        if (res.fail()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }
  }

  auto ctx = co_await mgr->leaseManagedTrx(tid, mode, /*isSideUser*/ false);
  if (!ctx) {
    LOG_TOPIC("2cfed", DEBUG, Logger::TRANSACTIONS)
        << "Transaction with id '" << tid << "' not found";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_NOT_FOUND,
        absl::StrCat("transaction '", tid.id(), "' not found"));
  }
  ctx->setStreaming();
  co_return ctx;
}
