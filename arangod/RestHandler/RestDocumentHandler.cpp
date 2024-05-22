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

#include "RestDocumentHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDocumentHandler::RestDocumentHandler(ArangodServer& server,
                                         GeneralRequest* request,
                                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestDocumentHandler::~RestDocumentHandler() = default;

RequestLane RestDocumentHandler::lane() const {
  if (ServerState::instance()->isDBServer()) {
    if (_request->requestType() == rest::RequestType::GET) {
      if (!_request->header(StaticStrings::AqlDocumentCall).empty()) {
        // DOCUMENT() function call from inside an AQL query. this will only
        // read data and does not need to wait on other requests. we will
        // give this somewhat higher priority because finishing this request
        // can unblock others.
        static_assert(PriorityRequestLane(RequestLane::CLUSTER_AQL_DOCUMENT) ==
                          RequestPriority::MED,
                      "invalid request lane priority");
        return RequestLane::CLUSTER_AQL_DOCUMENT;
      }
      // fall through for non-DOCUMENT() GET requests
    } else {
      // non-GET requests
      bool isSyncReplication = false;
      // We do not care for the real value, enough if it is there.
      std::ignore = _request->value(
          StaticStrings::IsSynchronousReplicationString, isSyncReplication);
      if (isSyncReplication) {
        return RequestLane::SERVER_SYNCHRONOUS_REPLICATION;
        // This leads to the high queue, we want replication requests to be
        // executed with a higher prio than leader requests, even if they
        // are done from AQL.
      }

      // fall through for not-GET, non-replication requests
    }
  }
  return RequestLane::CLIENT_SLOW;
}

RestStatus RestDocumentHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (!suffixes.empty()) {
    TRI_IF_FAILURE("failOnCRUDAPI" + suffixes[0]) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_DEBUG),
                    TRI_ERROR_DEBUG, "Intentional test error");
    }
  }
#endif
  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::DELETE_REQ:
      return waitForFuture(removeDocument());
    case rest::RequestType::GET:
      return readDocument();
    case rest::RequestType::HEAD:
      return checkDocument();
    case rest::RequestType::POST:
      return waitForFuture(insertDocument());
    case rest::RequestType::PUT:
      return replaceDocument();
    case rest::RequestType::PATCH:
      return updateDocument();
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
    }
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestDocumentHandler::shutdownExecute(bool isFinalized) noexcept {
  if (isFinalized) {
    TRI_ASSERT(_request != nullptr);
    TRI_ASSERT(_response != nullptr);
    try {
      auto const type = _request->requestType();
      auto const result = _response->responseCode();

      switch (type) {
        case rest::RequestType::DELETE_REQ:
        case rest::RequestType::GET:
        case rest::RequestType::HEAD:
        case rest::RequestType::POST:
        case rest::RequestType::PUT:
        case rest::RequestType::PATCH:
          break;
        default:
          events::IllegalDocumentOperation(*_request, result);
          break;
      }
    } catch (...) {
    }
  }

  RestVocbaseBaseHandler::shutdownExecute(isFinalized);
}

futures::Future<futures::Unit> RestDocumentHandler::insertDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    co_return;
  }

  bool found;
  std::string cname;
  if (suffixes.size() == 1) {
    cname = suffixes[0];
    found = true;
  } else {
    cname = _request->value("collection", found);
  }

  if (!found || cname.empty()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                      " POST /_api/document/<collection> or query parameter "
                      "'collection'");
    co_return;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    co_return;
  }

  arangodb::OperationOptions opOptions(_context);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);
  opOptions.versionAttribute =
      _request->value(StaticStrings::VersionAttributeString);
  opOptions.isRestore =
      _request->parsedValue(StaticStrings::IsRestoreString, false);
  opOptions.waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.validate =
      !_request->parsedValue(StaticStrings::SkipDocumentValidation, false);
  opOptions.returnNew =
      _request->parsedValue(StaticStrings::ReturnNewString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  handleFillIndexCachesValue(opOptions);

  if (_request->parsedValue(StaticStrings::Overwrite, false)) {
    // the default behavior if just "overwrite" is set
    opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
  }

  std::string const& mode = _request->value(StaticStrings::OverwriteMode);
  if (!mode.empty()) {
    auto overwriteMode =
        OperationOptions::determineOverwriteMode(std::string_view(mode));

    if (overwriteMode != OperationOptions::OverwriteMode::Unknown) {
      opOptions.overwriteMode = overwriteMode;

      if (opOptions.overwriteMode == OperationOptions::OverwriteMode::Update) {
        opOptions.mergeObjects =
            _request->parsedValue(StaticStrings::MergeObjectsString, true);
        opOptions.keepNull =
            _request->parsedValue(StaticStrings::KeepNullString, false);
      }
    }
  }

  opOptions.returnOld =
      _request->parsedValue(StaticStrings::ReturnOldString, false) &&
      opOptions.isOverwriteModeUpdateReplace();

  TRI_IF_FAILURE("delayed_synchronous_replication_request_processing") {
    if (!opOptions.isSynchronousReplicationFrom.empty()) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }
  }

  bool const isMultiple = body.isArray();
  transaction::Options trxOpts;
  trxOpts.delaySnapshot = !isMultiple;  // for now we only enable this for
                                        // single document operations

  auto trx = co_await createTransaction(
      cname, AccessMode::Type::WRITE, opOptions,
      transaction::OperationOriginREST{"inserting document(s)"},
      std::move(trxOpts));

  addTransactionHints(*trx, cname, isMultiple,
                      opOptions.isOverwriteModeUpdateReplace());

  Result res = co_await trx->beginAsync();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  if (ServerState::instance()->isDBServer() &&
      (trx->state()->collection(cname, AccessMode::Type::WRITE) == nullptr ||
       trx->state()->isReadOnlyTransaction())) {
    // make sure that the current transaction includes the collection that we
    // want to write into. this is not necessarily the case for follower
    // transactions that are started lazily. in this case, we must reject the
    // request. we _cannot_ do this for follower transactions, where shards may
    // lazily be added (e.g. if servers A and B both replicate their own write
    // ops to follower C one after the after, then C will first see only shards
    // from A and then only from B).
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
        absl::StrCat("Transaction with id '", trx->tid().id(),
                     "' does not contain collection '", cname,
                     "' with the required access mode."));
  }

  // track request only on leader
  if (opOptions.isSynchronousReplicationFrom.empty() &&
      ServerState::instance()->isDBServer()) {
    trx->state()->trackShardRequest(*trx->resolver(), _vocbase.name(), cname,
                                    _request->value(StaticStrings::UserString),
                                    AccessMode::Type::WRITE, "insert");
  }

  OperationResult opres = co_await trx->insertAsync(cname, body, opOptions);

  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = co_await trx->finishAsync(opres.result);
  if (opres.fail()) {
    generateTransactionError(cname, opres);
    co_return;
  }

  if (res.fail()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  generate20x(opres, cname, trx->getCollectionType(cname),
              trx->transactionContextPtr()->getVPackOptions(), isMultiple,
              opOptions.silent, rest::ResponseCode::CREATED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::readDocument() {
  size_t const len = _request->suffixes().size();

  switch (len) {
    case 0:
    case 1:
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    "expecting GET /_api/document/<collection>/<key>");
      return RestStatus::DONE;
    case 2:
      return waitForFuture(readSingleDocument(true));

    default:
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<collection>/<key>");
      return RestStatus::DONE;
  }
}

futures::Future<futures::Unit> RestDocumentHandler::readSingleDocument(
    bool generateBody) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  // split the document reference
  std::string const& collection = suffixes[0];

  std::string const& key = suffixes[1];

  // check for an etag
  bool isValidRevision;
  RevisionId ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid = RevisionId::max();  // an impossible rev, so precondition failed
                                    // will happen
  }

  OperationOptions options(_context);
  options.ignoreRevs = true;

  // Check if dirty reads are allowed:
  // This will be used in `createTransaction` below, if that creates
  // a new transaction. Otherwise, we use the default given by the
  // existing transaction.
  bool found = false;
  std::string const& val =
      _request->header(StaticStrings::AllowDirtyReads, found);
  if (found && StringUtils::boolean(val)) {
    // This will be used in `createTransaction` below, if that creates
    // a new transaction. Otherwise, we use the default given by the
    // existing transaction.
    options.allowDirtyReads = true;
  }

  RevisionId ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid = RevisionId::max();  // an impossible rev, so precondition failed
                                // will happen
  }

  auto buffer = std::make_shared<VPackBuffer<uint8_t>>();
  VPackBuilder builder(buffer);
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (ifRid.isSet()) {
      options.ignoreRevs = false;
      builder.add(StaticStrings::RevString, VPackValue(ifRid.toString()));
    }
  }

  VPackSlice search = builder.slice();

  // find collection given by name or identifier
  auto trx = co_await createTransaction(
      collection, AccessMode::Type::READ, options,
      transaction::OperationOriginREST{"fetching document"});

  trx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = co_await trx->beginAsync();

  if (!res.ok()) {
    generateTransactionError(collection, OperationResult(res, options), "");
    co_return;
  }

  // track request on both leader and follower (in case of dirty-read requests)
  if (ServerState::instance()->isDBServer()) {
    trx->state()->trackShardRequest(*trx->resolver(), _vocbase.name(),
                                    collection,
                                    _request->value(StaticStrings::UserString),
                                    AccessMode::Type::READ, "read");
  }

  if (trx->state()->options().allowDirtyReads) {
    setOutgoingDirtyReadsHeader(true);
  }

  OperationResult opRes =
      co_await trx->documentAsync(collection, search, options);
  res = co_await trx->finishAsync(opRes.result);
  if (!opRes.ok()) {
    generateTransactionError(collection, opRes, key, ifRid);
    co_return;
  }

  if (!res.ok()) {
    generateTransactionError(collection, OperationResult(res, options), key,
                             ifRid);
    co_return;
  }

  if (ifNoneRid.isSet()) {
    RevisionId const rid = RevisionId::fromSlice(opRes.slice());
    if (ifNoneRid == rid) {
      generateNotModified(rid);
      co_return;
    }
  }

  // use default options
  generateDocument(opRes.slice(), generateBody,
                   trx->transactionContextPtr()->getVPackOptions());
}

RestStatus RestDocumentHandler::checkDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting HEAD /_api/document/<collection>/<key>");
    return RestStatus::DONE;
  }

  return waitForFuture(readSingleDocument(false));
}

RestStatus RestDocumentHandler::replaceDocument() {
  bool found;
  _request->value("onlyget", found);
  if (found) {
    return waitForFuture(readManyDocuments());
  }
  return waitForFuture(modifyDocument(false));
}

RestStatus RestDocumentHandler::updateDocument() {
  return waitForFuture(modifyDocument(true));
}

futures::Future<futures::Unit> RestDocumentHandler::modifyDocument(
    bool isPatch) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 2) {
    std::string msg("expecting ");
    msg.append(isPatch ? "PATCH" : "PUT");
    msg.append(
        " /_api/document/<collection> or"
        " /_api/document/<collection>/<key> or"
        " /_api/document and query parameter 'collection'");

    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    co_return;
  }

  bool isArrayCase = suffixes.size() <= 1;

  std::string cname;
  std::string key;

  if (isArrayCase) {
    bool found;
    if (suffixes.size() == 1) {
      cname = suffixes[0];
      found = true;
    } else {
      cname = _request->value("collection", found);
    }
    if (!found) {
      std::string msg(
          "collection must be given in URL path or query parameter "
          "'collection' must be specified");
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
      co_return;
    }
  } else {
    cname = suffixes[0];
    key = suffixes[1];
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    co_return;
  }

  OperationOptions opOptions(_context);
  if ((!isArrayCase && !body.isObject()) || (isArrayCase && !body.isArray())) {
    generateTransactionError(
        cname,
        OperationResult(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID, opOptions), "");
    co_return;
  }

  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);
  opOptions.versionAttribute =
      _request->value(StaticStrings::VersionAttributeString);
  opOptions.isRestore =
      _request->parsedValue(StaticStrings::IsRestoreString, false);
  opOptions.ignoreRevs =
      _request->parsedValue(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.validate =
      !_request->parsedValue(StaticStrings::SkipDocumentValidation, false);
  opOptions.returnNew =
      _request->parsedValue(StaticStrings::ReturnNewString, false);
  opOptions.returnOld =
      _request->parsedValue(StaticStrings::ReturnOldString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  handleFillIndexCachesValue(opOptions);

  TRI_IF_FAILURE("delayed_synchronous_replication_request_processing") {
    if (!opOptions.isSynchronousReplicationFrom.empty()) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }
  }

  // extract the revision, if single document variant and header given:
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;
  RevisionId headerRev = RevisionId::none();
  if (!isArrayCase) {
    bool isValidRevision;
    headerRev = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      headerRev =
          RevisionId::max();  // an impossible revision, so precondition failed
    }
    if (headerRev.isSet()) {
      opOptions.ignoreRevs = false;
    }

    VPackSlice keyInBody = body.get(StaticStrings::KeyString);
    RevisionId revInBody = RevisionId::fromSlice(body);
    if ((headerRev.isSet() && revInBody != headerRev) || keyInBody.isNone() ||
        keyInBody.isNull() ||
        (keyInBody.isString() && keyInBody.stringView() != key)) {
      // We need to rewrite the document with the given revision and key:
      buffer = std::make_shared<VPackBuffer<uint8_t>>();
      VPackBuilder builder(buffer);
      {
        VPackObjectBuilder guard(&builder);
        TRI_SanitizeObject(body, builder);
        builder.add(StaticStrings::KeyString, VPackValue(key));
        if (headerRev.isSet()) {
          builder.add(StaticStrings::RevString,
                      VPackValue(headerRev.toString()));
        } else if (!opOptions.ignoreRevs && revInBody.isSet()) {
          builder.add(StaticStrings::RevString,
                      VPackValue(revInBody.toString()));
          headerRev = revInBody;  // make sure that we report 412 and not 409
        }
      }

      body = builder.slice();
    } else if (!headerRev.isSet() && revInBody.isSet() &&
               opOptions.ignoreRevs == false) {
      headerRev = revInBody;  // make sure that we report 412 and not 409
    }
  }

  bool const isMultiple = body.isArray();
  transaction::Options trxOpts;
  trxOpts.delaySnapshot = !isMultiple;  // for now we only enable this for
                                        // single document operations

  // find collection given by name or identifier
  auto trx = co_await createTransaction(
      cname, AccessMode::Type::WRITE, opOptions,
      transaction::OperationOriginREST{"modifying document(s)"},
      std::move(trxOpts));

  addTransactionHints(*trx, cname, isArrayCase, false);

  // ...........................................................................
  // inside write transaction
  // ...........................................................................

  Result res = co_await trx->beginAsync();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  // track request only on leader
  if (opOptions.isSynchronousReplicationFrom.empty() &&
      ServerState::instance()->isDBServer()) {
    trx->state()->trackShardRequest(*trx->resolver(), _vocbase.name(), cname,
                                    _request->value(StaticStrings::UserString),
                                    AccessMode::Type::WRITE,
                                    isPatch ? "update" : "replace");
  }

  if (ServerState::instance()->isDBServer() &&
      (trx->state()->collection(cname, AccessMode::Type::WRITE) == nullptr ||
       trx->state()->isReadOnlyTransaction())) {
    // make sure that the current transaction includes the collection that we
    // want to write into. this is not necessarily the case for follower
    // transactions that are started lazily. in this case, we must reject the
    // request. we _cannot_ do this for follower transactions, where shards may
    // lazily be added (e.g. if servers A and B both replicate their own write
    // ops to follower C one after the after, then C will first see only shards
    // from A and then only from B).
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
        absl::StrCat("Transaction with id '", trx->tid().id(),
                     "' does not contain collection '", cname,
                     "' with the required access mode."));
  }

  auto f = futures::Future<OperationResult>::makeEmpty();
  if (isPatch) {
    // patching an existing document
    opOptions.keepNull =
        _request->parsedValue(StaticStrings::KeepNullString, true);
    opOptions.mergeObjects =
        _request->parsedValue(StaticStrings::MergeObjectsString, true);
    f = trx->updateAsync(cname, body, opOptions);
  } else {
    f = trx->replaceAsync(cname, body, opOptions);
  }

  OperationResult opRes = co_await std::move(f);
  res = co_await trx->finishAsync(opRes.result);
  if (opRes.fail()) {
    generateTransactionError(cname, opRes, key, headerRev);
    co_return;
  }

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), key,
                             headerRev);
    co_return;
  }

  generate20x(opRes, cname, trx->getCollectionType(cname),
              trx->transactionContextPtr()->getVPackOptions(), isArrayCase,
              opOptions.silent, rest::ResponseCode::CREATED);
}

futures::Future<futures::Unit> RestDocumentHandler::removeDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() < 1 || suffixes.size() > 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<collection>/<key> or "
                  "/_api/document/<collection> with a body");
    co_return;
  }

  // split the document reference
  std::string const& cname = suffixes[0];

  std::string key;
  if (suffixes.size() == 2) {
    key = suffixes[1];
  }

  // extract the revision if single document case
  RevisionId revision = RevisionId::none();
  if (suffixes.size() == 2) {
    bool isValidRevision = false;
    revision = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      revision =
          RevisionId::max();  // an impossible revision, so precondition failed
    }
  }

  OperationOptions opOptions(_context);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);
  opOptions.returnOld =
      _request->parsedValue(StaticStrings::ReturnOldString, false);
  opOptions.ignoreRevs =
      _request->parsedValue(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  handleFillIndexCachesValue(opOptions);

  TRI_IF_FAILURE("delayed_synchronous_replication_request_processing") {
    if (!opOptions.isSynchronousReplicationFrom.empty()) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }
  }

  VPackSlice search;
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;

  if (suffixes.size() == 2) {
    buffer = std::make_shared<VPackBuffer<uint8_t>>();
    VPackBuilder builder(buffer);
    {
      VPackObjectBuilder guard(&builder);

      builder.add(StaticStrings::KeyString, VPackValue(key));

      if (revision.isSet()) {
        opOptions.ignoreRevs = false;
        builder.add(StaticStrings::RevString, VPackValue(revision.toString()));
      }
    }

    search = builder.slice();
  } else {
    bool parseSuccess = false;
    search = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      co_return;
    }
  }

  if (!search.isArray() && !search.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Request body not parseable");
    co_return;
  }

  bool const isMultiple = search.isArray();
  transaction::Options trxOpts;
  trxOpts.delaySnapshot = !isMultiple;  // for now we only enable this for
                                        // single document operations

  auto trx = co_await createTransaction(
      cname, AccessMode::Type::WRITE, opOptions,
      transaction::OperationOriginREST{"removing document(s)"},
      std::move(trxOpts));

  addTransactionHints(*trx, cname, isMultiple, false);

  Result res = co_await trx->beginAsync();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  // track request only on leader
  if (opOptions.isSynchronousReplicationFrom.empty() &&
      ServerState::instance()->isDBServer()) {
    trx->state()->trackShardRequest(*trx->resolver(), _vocbase.name(), cname,
                                    _request->value(StaticStrings::UserString),
                                    AccessMode::Type::WRITE, "remove");
  }

  if (ServerState::instance()->isDBServer() &&
      (trx->state()->collection(cname, AccessMode::Type::WRITE) == nullptr ||
       trx->state()->isReadOnlyTransaction())) {
    // make sure that the current transaction includes the collection that we
    // want to write into. this is not necessarily the case for follower
    // transactions that are started lazily. in this case, we must reject the
    // request. we _cannot_ do this for follower transactions, where shards may
    // lazily be added (e.g. if servers A and B both replicate their own write
    // ops to follower C one after the after, then C will first see only shards
    // from A and then only from B).
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
        absl::StrCat("Transaction with id '", trx->tid().id(),
                     "' does not contain collection '", cname,
                     "' with the required access mode."));
  }

  OperationResult opRes = co_await trx->removeAsync(cname, search, opOptions);
  res = co_await trx->finishAsync(opRes.result);
  if (opRes.fail()) {
    generateTransactionError(cname, opRes, key, revision);
    co_return;
  }

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), key);
    co_return;
  }

  generate20x(opRes, cname, trx->getCollectionType(cname),
              trx->transactionContextPtr()->getVPackOptions(), isMultiple,
              opOptions.silent, rest::ResponseCode::OK);
}

futures::Future<futures::Unit> RestDocumentHandler::readManyDocuments() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/document/<collection> with a body");
    co_return;
  }

  // split the document reference
  std::string const& cname = suffixes[0];

  OperationOptions opOptions(_context);
  opOptions.ignoreRevs =
      _request->parsedValue(StaticStrings::IgnoreRevsString, true);

  // Check if dirty reads are allowed:
  bool found = false;
  std::string const& val =
      _request->header(StaticStrings::AllowDirtyReads, found);
  if (found && StringUtils::boolean(val)) {
    opOptions.allowDirtyReads = true;
    // This will tell `createTransaction` below, that in the case it
    // actually creates a new transaction (rather than using an existing
    // one), we want to read from followers. If the transaction is already
    // there, the flag is ignored.
  }

  bool success;
  VPackSlice search = this->parseVPackBody(success);
  if (!success) {  // error message generated in parseVPackBody
    co_return;
  }

  auto trx = co_await createTransaction(
      cname, AccessMode::Type::READ, opOptions,
      transaction::OperationOriginREST{"fetching documents"});

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = co_await trx->beginAsync();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  // track request on both leader and follower (in case of dirty-read requests)
  if (ServerState::instance()->isDBServer()) {
    trx->state()->trackShardRequest(*trx->resolver(), _vocbase.name(), cname,
                                    _request->value(StaticStrings::UserString),
                                    AccessMode::Type::READ, "read-multiple");
  }

  if (trx->state()->options().allowDirtyReads) {
    setOutgoingDirtyReadsHeader(true);
  }

  OperationResult opRes = co_await trx->documentAsync(cname, search, opOptions);
  res = co_await trx->finishAsync(opRes.result);

  if (opRes.fail()) {
    generateTransactionError(cname, opRes);
    co_return;
  }

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    co_return;
  }

  generateDocument(opRes.slice(), true,
                   trx->transactionContextPtr()->getVPackOptions());
}

void RestDocumentHandler::handleFillIndexCachesValue(
    OperationOptions& options) {
  RefillIndexCaches ric = RefillIndexCaches::kDefault;

  if (!options.isSynchronousReplicationFrom.empty() &&
      !_vocbase.server()
           .template getFeature<EngineSelectorFeature>()
           .engine()
           .autoRefillIndexCachesOnFollowers()) {
    // do not refill caches on followers if this is intentionally turned off
    ric = RefillIndexCaches::kDontRefill;
  } else {
    bool found = false;
    std::string const& value =
        _request->value(StaticStrings::RefillIndexCachesString, found);
    if (found) {
      // this attribute can have 3 values: default, true and false. only
      // pick it up when it is set to true or false
      ric = StringUtils::boolean(value) ? RefillIndexCaches::kRefill
                                        : RefillIndexCaches::kDontRefill;
    }
  }

  options.refillIndexCaches = ric;
}

void RestDocumentHandler::addTransactionHints(transaction::Methods& trx,
                                              std::string_view collectionName,
                                              bool isMultiple,
                                              bool isOverwritingInsert) {
  if (ServerState::instance()->isCoordinator()) {
    CollectionNameResolver resolver{_vocbase};
    auto col = resolver.getCollection(collectionName);
    if (col != nullptr && col->isSmartEdgeCollection()) {
      // Smart Edge Collections hit multiple shards with dependent requests,
      // they have to be globally managed.
      trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
      return;
    }
  }
  // For non multiple operations we can optimize to use SingleOperations.
  if (!isMultiple && !isOverwritingInsert) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }
}
