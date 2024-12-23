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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestCursorHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/SharedQueryState.h"
#include "Async/async.h"
#include "Basics/dtrace-wrapper.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/OperationOrigin.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestCursorHandler::RestCursorHandler(
    ArangodServer& server, GeneralRequest* request, GeneralResponse* response,
    arangodb::aql::QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(server, request, response),
      _queryKilled(false),
      _queryRegistry(queryRegistry),
      _cursor(nullptr) {}

RestCursorHandler::~RestCursorHandler() { releaseCursor(); }

RequestLane RestCursorHandler::lane() const {
  if (_request->requestType() != rest::RequestType::POST ||
      !_request->suffixes().empty()) {
    // continuing an existing query or cleaning up its resources
    // gets higher priority than starting new queries
    return RequestLane::CONTINUATION;
  }

  // low priority for starting new queries
  return RequestLane::CLIENT_AQL;
}

RestStatus RestCursorHandler::execute() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

futures::Future<futures::Unit> RestCursorHandler::executeAsync() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    if (_request->suffixes().size() == 0) {
      // POST /_api/cursor
      co_return co_await createQueryCursor();
    } else if (_request->suffixes().size() == 1) {
      // POST /_api/cursor/cursor-id
      co_return co_await modifyQueryCursor();
    }
    // POST /_api/cursor/cursor-id/batch-id
    co_return co_await showLatestBatch();
  } else if (type == rest::RequestType::PUT) {
    co_return co_await modifyQueryCursor();
  } else if (type == rest::RequestType::DELETE_REQ) {
    TRI_ASSERT(deleteQueryCursor() == RestStatus::DONE);
    co_return;
  }
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  co_return;
}

RestStatus RestCursorHandler::continueExecute() {
  TRI_ASSERT(false) << "not implemented";
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void RestCursorHandler::shutdownExecute(bool isFinalized) noexcept {
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { RestVocbaseBaseHandler::shutdownExecute(isFinalized); });

  // request not done yet
  if (!isFinalized) {
    return;
  }

  releaseCursor();

  // only trace create cursor requests
  if (_request->requestType() != rest::RequestType::POST ||
      _request->suffixes().size() > 0) {
    return;
  }

  // destroy the query context.
  // this is needed because the context is managing resources (e.g. leases
  // for a managed transaction) that we want to free as early as possible
  _queryResult.context.reset();
}

void RestCursorHandler::cancel() {
  RestVocbaseBaseHandler::cancel();
  return cancelQuery();
}

/// @brief register the query either as streaming cursor or in _query
/// the query is not executed here.
/// this method is also used by derived classes
///
/// return If true, we need to continue processing,
///        If false we are done (error or stream)
auto RestCursorHandler::registerQueryOrCursor(
    velocypack::Slice slice, transaction::OperationOrigin operationOrigin)
    -> async<void> {
  TRI_ASSERT(_query == nullptr);

  if (!slice.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    co_return;
  }
  VPackSlice querySlice = slice.get("query");
  if (!querySlice.isString() || querySlice.getStringLength() == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    co_return;
  }

  VPackSlice bindVars = slice.get("bindVars");
  if (!bindVars.isNone()) {
    if (!bindVars.isObject() && !bindVars.isNull()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting object for <bindVars>");
      co_return;
    }
  }

  std::shared_ptr<VPackBuilder> bindVarsBuilder;
  if (!bindVars.isNone()) {
    bindVarsBuilder = std::make_shared<VPackBuilder>(bindVars);
  }

  TRI_ASSERT(_options == nullptr);
  buildOptions(slice);
  TRI_ASSERT(_options != nullptr);
  VPackSlice opts = _options->slice();

  bool stream = VelocyPackHelper::getBooleanValue(opts, "stream", false);
  if (ServerState::instance()->isDBServer()) {
    stream = false;
  }
  size_t batchSize =
      VelocyPackHelper::getNumericValue<size_t>(opts, "batchSize", 1000);
  double ttl = VelocyPackHelper::getNumericValue<double>(
      opts, "ttl", _queryRegistry->defaultTTL());
  bool count = VelocyPackHelper::getBooleanValue(opts, "count", false);
  bool retriable = VelocyPackHelper::getBooleanValue(opts, "allowRetry", false);

  // simon: access mode can always be write on the coordinator
  AccessMode::Type mode = AccessMode::Type::WRITE;

  auto query = aql::Query::create(
      co_await createTransactionContext(mode, operationOrigin),
      aql::QueryString(querySlice.stringView()), std::move(bindVarsBuilder),
      aql::QueryOptions(opts));

  if (stream) {
    TRI_ASSERT(!ServerState::instance()->isDBServer());
    if (count) {
      generateError(Result(TRI_ERROR_BAD_PARAMETER,
                           "cannot use 'count' option for a streaming query"));
      co_return;
    }

    CursorRepository* cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    _cursor = co_await cursors->createQueryStream(std::move(query), batchSize,
                                                  ttl, retriable);
    // Throws if soft shutdown is ongoing!
    // TODO don't call setWakeupHandler at all
    _cursor->setWakeupHandler(withLogContext([]() -> bool { std::abort(); }));

    co_return co_await generateCursorResult(rest::ResponseCode::CREATED);
  }

  // non-stream case. Execute query, then build a cursor
  // with the entire result set.
  if (!ServerState::instance()->isDBServer()) {
    auto ss = query->sharedState();
    TRI_ASSERT(ss != nullptr);
    if (ss == nullptr) {
      generateError(Result(TRI_ERROR_INTERNAL, "invalid query state"));
      co_return;
    }

    // TODO don't call setWakeupHandler at all
    ss->setWakeupHandler(withLogContext(
        [self = shared_from_this()]() -> bool { std::abort(); }));
  }

  registerQuery(std::move(query));
  co_return co_await processQuery();
}

/// @brief Process the query registered in _query.
/// The function is repeatable, so whenever we need to WAIT
/// in AQL we can post a handler calling this function again.
auto RestCursorHandler::processQuery() -> async<void> {
  auto query = [this]() {
    std::unique_lock<std::mutex> mutexLocker{_queryLock};

    if (_query == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Illegal state in RestCursorHandler, query not found.");
    }
    return _query;
  }();

  {
    // always clean up
    auto guard = scopeGuard([this]() noexcept { unregisterQuery(); });

    // continue handler is registered earlier
    co_await query->execute(_queryResult);
  }

  // We cannot get into HASMORE here, or we would lose results.
  co_return co_await handleQueryResult();
}

// non stream case, result is complete
auto RestCursorHandler::handleQueryResult() -> async<void> {
  TRI_ASSERT(_query == nullptr);
  if (_queryResult.result.fail()) {
    if (_queryResult.result.is(TRI_ERROR_REQUEST_CANCELED) ||
        (_queryResult.result.is(TRI_ERROR_QUERY_KILLED) && wasCanceled())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION(_queryResult.result);
  }

  VPackSlice qResult = _queryResult.data->slice();

  if (qResult.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_ASSERT(qResult.isArray());
  TRI_ASSERT(_options != nullptr);
  VPackSlice opts = _options->slice();

  size_t batchSize =
      VelocyPackHelper::getNumericValue<size_t>(opts, "batchSize", 1000);
  double ttl = VelocyPackHelper::getNumericValue<double>(opts, "ttl", 30);
  bool count = VelocyPackHelper::getBooleanValue(opts, "count", false);
  bool retriable = VelocyPackHelper::getBooleanValue(opts, "allowRetry", false);

  _response->setContentType(rest::ContentType::JSON);
  size_t n = static_cast<size_t>(qResult.length());
  if (n <= batchSize) {
    // result is smaller than batchSize and will be returned directly. no need
    // to create a cursor

    if (_queryResult.allowDirtyReads) {
      setOutgoingDirtyReadsHeader(true);
    }

    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;

    // conservatively allocate a few bytes per value to be returned
    auto res = TRI_ERROR_NO_ERROR;
    if (n >= 10000) {
      res = _response->reservePayload(128 * 1024);
    } else if (n >= 1000) {
      res = _response->reservePayload(64 * 1024);
    } else {
      res = _response->reservePayload(n * 48);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackBuffer<uint8_t> buffer;
    VPackBuilder result(buffer, &options);
    try {
      VPackObjectBuilder b(&result);
      result.add(VPackValue("result"));
      result.add(VPackValue(qResult.begin(), VPackValueType::External));
      result.add("hasMore", VPackValue(false));
      if (VelocyPackHelper::getBooleanValue(opts, "count", false)) {
        result.add("count", VPackValue(n));
      }
      result.add("cached", VPackValue(_queryResult.cached));
      if (!_queryResult.extra) {
        result.add("extra", VPackValue(VPackValueType::Object));
        // no warnings
        result.add("warnings", VPackSlice::emptyArraySlice());
        result.close();
      } else {
        result.add("extra", _queryResult.extra->slice());
      }
      if (_queryResult.planCacheKey.has_value()) {
        result.add(
            "planCacheKey",
            VPackValue(std::to_string(_queryResult.planCacheKey.value())));
      }
      result.add(StaticStrings::Error, VPackValue(false));
      result.add(StaticStrings::Code,
                 VPackValue(static_cast<int>(ResponseCode::CREATED)));
    } catch (std::exception const& ex) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    generateResult(rest::ResponseCode::CREATED, std::move(buffer),
                   _queryResult.context);
    // directly after returning from here, we will free the query's context and
    // free the resources it uses (e.g. leases for a managed transaction). this
    // way the server can send back the query result to the client and the
    // client can make follow-up requests on the same transaction (e.g.
    // trx.commit()) without the server code for freeing the resources and the
    // client code racing for who's first

    co_return;
  } else {
    // result is bigger than batchSize, and a cursor will be created
    CursorRepository* cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    TRI_ASSERT(_queryResult.data.get() != nullptr);
    // steal the query result, cursor will take over the ownership
    _cursor = cursors->createFromQueryResult(std::move(_queryResult), batchSize,
                                             ttl, count, retriable);
    // throws if a coordinator soft shutdown is ongoing

    co_return co_await generateCursorResult(rest::ResponseCode::CREATED);
  }
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestCursorHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  rest::RequestType type = _request->requestType();
  if (type != rest::RequestType::POST && type != rest::RequestType::PUT &&
      type != rest::RequestType::DELETE_REQ) {
    // request forwarding only exists for
    // POST /_api/cursor/cursor-id
    // PUT /_api/cursor/cursor-id
    // DELETE /_api/cursor/cursor-id
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  if (sourceServer == ServerState::instance()->getShortId()) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto coordinatorId = ci.getCoordinatorByShortID(sourceServer);

  if (coordinatorId.empty()) {
    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_CURSOR_NOT_FOUND, "cannot find target server for cursor id");
  }

  return {std::make_pair(std::move(coordinatorId), false)};
}

/// @brief register the currently running query
void RestCursorHandler::registerQuery(
    std::shared_ptr<arangodb::aql::Query> query) {
  std::lock_guard mutexLocker{_queryLock};

  if (_queryKilled) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
  }

  TRI_ASSERT(_query == nullptr);
  TRI_ASSERT(query != nullptr);
  _query = std::move(query);
}

/// @brief unregister the currently running query
void RestCursorHandler::unregisterQuery() noexcept {
  TRI_IF_FAILURE(
      "RestCursorHandler::directKillBeforeQueryResultIsGettingHandled") {
    _query->debugKillQuery();
  }
  std::lock_guard mutexLocker{_queryLock};
  _query.reset();
}

/// @brief cancel the currently running query
void RestCursorHandler::cancelQuery() {
  std::lock_guard mutexLocker{_queryLock};

  if (_query != nullptr) {
    // cursor is canceled. now remove the continue handler we may have
    // registered in the query
    if (_query->sharedState()) {
      _query->sharedState()->resetWakeupHandler();
    }

    _query->setKillFlag();
  }
  _queryKilled = true;
}

bool RestCursorHandler::wasCanceled() const {
  std::lock_guard mutexLocker{_queryLock};
  return _queryKilled;
}

/// @brief build options for the query as JSON
void RestCursorHandler::buildOptions(velocypack::Slice slice) {
  _options = std::make_shared<VPackBuilder>();
  VPackObjectBuilder obj(_options.get());

  bool hasCache = false;
  bool hasMemoryLimit = false;
  VPackSlice opts = slice.get("options");
  // "stream" option is important, so also accept it on top level and not only
  // inside options
  bool isStream = VelocyPackHelper::getBooleanValue(slice, "stream", false);

  // Look for allowDirtyReads header:
  bool allowDirtyReads = false;
  bool found = false;
  bool sawDirtyReads = false;
  std::string const& val =
      _request->header(StaticStrings::AllowDirtyReads, found);
  if (found && StringUtils::boolean(val)) {
    allowDirtyReads = true;
  }

  if (opts.isObject()) {
    if (!isStream) {
      isStream = VelocyPackHelper::getBooleanValue(opts, "stream", false);
    }
    for (auto it : VPackObjectIterator(opts)) {
      if (!it.key.isString() || it.value.isNone()) {
        continue;
      }
      std::string_view keyName = it.key.stringView();
      if (keyName == "allowDirtyReads") {
        // We will set the flag in the options, if either the HTTP header
        // is set, or we see true here in the slice:
        sawDirtyReads = true;
        allowDirtyReads |= it.value.isTrue();
        _options->add(keyName, VPackValue(allowDirtyReads));
      }
      if (keyName == "count" || keyName == "batchSize" || keyName == "ttl" ||
          keyName == "stream" || (isStream && keyName == "fullCount")) {
        continue;  // filter out top-level keys
      } else if (keyName == "cache") {
        hasCache = true;  // don't honor if appears below
      } else if (keyName == "memoryLimit" && it.value.isNumber()) {
        hasMemoryLimit = true;
      }
      _options->add(keyName, it.value);
    }
  }
  if (!sawDirtyReads && allowDirtyReads) {
    _options->add("allowDirtyReads", VPackValue(true));
  }

  if (ServerState::instance()->isDBServer()) {
    // we do not support creating streaming cursors on DB-Servers, at all.
    // always turn such cursors into non-streaming cursors.
    isStream = false;
  }

  _options->add("stream", VPackValue(isStream));

  if (!isStream) {  // ignore cache & count for streaming queries
    bool val = VelocyPackHelper::getBooleanValue(slice, "count", false);
    _options->add("count", VPackValue(val));
    if (!hasCache && slice.hasKey("cache")) {
      val = VelocyPackHelper::getBooleanValue(slice, "cache", false);
      _options->add("cache", VPackValue(val));
    }
  }

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isDouble() && batchSize.getDouble() == 0.0) ||
        (batchSize.isInteger() && batchSize.getNumericValue<int64_t>() <= 0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting positive value for <batchSize>");
    }
    _options->add("batchSize", batchSize);
  } else {
    _options->add("batchSize", VPackValue(1000));
  }

  if (!hasMemoryLimit) {
    VPackSlice memoryLimit = slice.get("memoryLimit");
    if (memoryLimit.isNumber()) {
      _options->add("memoryLimit", memoryLimit);
    }
  }

  VPackSlice ttl = slice.get("ttl");
  _options->add("ttl", VPackValue(ttl.isNumber() && ttl.getNumber<double>() > 0
                                      ? ttl.getNumber<double>()
                                      : _queryRegistry->defaultTTL()));
}

/// @brief append the contents of the cursor into the response body
/// this function will also take care of the cursor and return it to the
/// registry if required
auto RestCursorHandler::generateCursorResult(rest::ResponseCode code)
    -> async<void> {
  TRI_ASSERT(_cursor != nullptr);

  // dump might delete the cursor
  std::shared_ptr<transaction::Context> ctx = _cursor->context();

  VPackBuilder builder;
  builder.openObject(/*unindexed*/ true);

  auto const [state, r] = co_await _cursor->dump(builder);
  TRI_ASSERT(state != aql::ExecutionState::WAITING);

  if (_cursor->allowDirtyReads()) {
    setOutgoingDirtyReadsHeader(true);
  }

  if (r.ok()) {
    builder.add(StaticStrings::Error, VPackValue(false));
    builder.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    builder.close();
    _response->setContentType(rest::ContentType::JSON);
    TRI_IF_FAILURE("MakeConnectionErrorForRetry") {
      if (_cursor->isRetriable()) {
        _cursor->setLastQueryBatchObject(builder.steal());
      }
      co_return;
    }

    generateResult(code, builder.slice(), std::move(ctx));
    if (_cursor->isRetriable()) {
      _cursor->setLastQueryBatchObject(builder.steal());
    }
  } else {
    if (_cursor->isRetriable()) {
      builder.add(StaticStrings::Code,
                  VPackValue(static_cast<int>(
                      GeneralResponse::responseCode(r.errorNumber()))));
      builder.add(StaticStrings::Error, VPackValue(true));
      builder.add(StaticStrings::ErrorMessage, VPackValue(r.errorMessage()));
      builder.add(StaticStrings::ErrorNum, VPackValue(r.errorNumber()));
      builder.close();
      _cursor->setLastQueryBatchObject(builder.steal());
    }
    // builder can be in a broken state here. simply return the error
    generateError(r);
  }

  co_return;
}

async<void> RestCursorHandler::createQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    co_return;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    // error message generated in parseVPackBody
    co_return;
  }

  if (body.isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    co_return;
  }

  TRI_ASSERT(_query == nullptr);
  co_await registerQueryOrCursor(
      body, transaction::OperationOriginAQL{"running AQL query"});
  co_return;
}

/// @brief shows the batch given by <batch-id> if it's the last cached batch
/// response on a retry, and does't advance cursor
auto RestCursorHandler::showLatestBatch() -> async<void> {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor/<cursor-id>/<batch-id>");
    co_return;
  }

  uint64_t batchId = basics::StringUtils::uint64(suffixes[1]);

  lookupCursor(suffixes[0], batchId);

  if (_cursor == nullptr) {
    // error response already built here
    co_return;
  }

  // TODO don't call setWakeupHandler at all
  _cursor->setWakeupHandler(
      withLogContext([self = shared_from_this()]() -> bool { std::abort(); }));

  // POST /_api/cursor/<cid>/x and the current batchId on the server is y, then:
  //   if x == y, resend the current batch
  //   if x == y + 1, advance the cursor and return the new batch
  //   otherwise return error
  if (_cursor->isNextBatchId(batchId)) {
    co_return co_await generateCursorResult(rest::ResponseCode::OK);
  }

  if (!_cursor->isCurrentBatchId(batchId)) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batch id not found");
    co_return;
  }

  auto buffer = _cursor->getLastBatch();
  TRI_ASSERT(buffer != nullptr);

  _response->setContentType(rest::ContentType::JSON);
  generateResult(rest::ResponseCode::OK, VPackSlice(buffer->data()),
                 _cursor->context());

  co_return;
}

auto RestCursorHandler::modifyQueryCursor() -> async<void> {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor/<cursor-id>");
    co_return;
  }

  // the call to lookupCursor will populate _cursor if the cursor can be
  // found. otherwise, _cursor will remain a nullptr and an error will
  // be written to the response
  lookupCursor(suffixes[0]);

  if (_cursor == nullptr) {
    co_return;
  }

  // TODO don't call setWakeupHandler at all
  _cursor->setWakeupHandler(
      withLogContext([self = shared_from_this()]() -> bool { std::abort(); }));

  co_return co_await generateCursorResult(rest::ResponseCode::OK);
}

RestStatus RestCursorHandler::deleteQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return RestStatus::DONE;
  }

  std::string const& id = suffixes[0];

  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId =
      static_cast<arangodb::CursorId>(basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return RestStatus::DONE;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(id));
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code,
              VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  builder.close();

  generateResult(rest::ResponseCode::ACCEPTED, builder.slice());
  return RestStatus::DONE;
}

/// @brief look up cursor by id. side-effect: populates _cursor in case cursor
/// was found. in case cursor was not found, writes an error into the response
void RestCursorHandler::lookupCursor(std::string_view id,
                                     std::optional<uint64_t> batchId) {
  TRI_ASSERT(_cursor == nullptr);

  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId =
      static_cast<arangodb::CursorId>(basics::StringUtils::uint64(id));
  bool busy;
  _cursor = cursors->find(cursorId, busy);

  if (_cursor == nullptr) {
    if (busy) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_BUSY),
                    TRI_ERROR_CURSOR_BUSY);
    } else {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }

  TRI_ASSERT(_cursor != nullptr);

  if (batchId.has_value() && _cursor->isCurrentBatchId(batchId.value()) &&
      !_cursor->isRetriable()) {
    releaseCursor();
    TRI_ASSERT(_cursor == nullptr);

    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting allowRetry option to be true");
  }
}

void RestCursorHandler::releaseCursor() {
  if (_cursor) {
    _cursor->resetWakeupHandler();

    auto cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    cursors->release(_cursor);
    _cursor = nullptr;
  }
}

void RestCursorHandler::runHandler(
    std::function<void(rest::RestHandler*)> responseCallback) {
  _sendResponseCallback = std::move(responseCallback);

  runHandlerStateMachineAsync().thenFinal(
      [self = shared_from_this()](auto&& tryResult) noexcept {
        // TODO handle exceptions
        std::move(tryResult).throwIfFailed();
      });
}

auto RestCursorHandler::runHandlerStateMachineAsync()
    -> futures::Future<futures::Unit> {
  auto fail = [&]() {
    TRI_ASSERT(_state == HandlerState::FAILED);
    _statistics.SET_REQUEST_END();
    // Callback may stealStatistics!
    _sendResponseCallback(this);

    shutdownExecute(false);
  };

  TRI_ASSERT(_state == HandlerState::PREPARE);
  auto logScope = prepareEngine();
  // TODO put logScope into scopeGuard
  // TODO If I understand correctly, this only works on the same thread.
  //      async<T> should pass on the LogContext to the next thread, so this can
  //      work here as well.
  std::vector<LogContext::Accessor::ScopedValue> scopeGuard;
  if (_state == HandlerState::FAILED) {
    co_return fail();
  }
  TRI_ASSERT(_state == HandlerState::EXECUTE);
  co_await executeEngineAsync();
  if (_state == HandlerState::FAILED) {
    co_return fail();
  }

  TRI_ASSERT(_state == HandlerState::FINALIZE);
  _statistics.SET_REQUEST_END();

  // shutdownExecute is noexcept
  shutdownExecute(true);  // may not be moved down

  _state = HandlerState::DONE;

  // compress response if required
  compressResponse();
  // Callback may stealStatistics!
  _sendResponseCallback(this);
}

auto RestCursorHandler::executeEngineAsync() -> async<void> {
  DTRACE_PROBE1(arangod, RestHandlerExecuteEngine, this);
  ExecContextScope scope(
      basics::downCast<ExecContext>(_request->requestContext()));

  try {
    co_await executeAsync();

    if (_response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler");
      handleError(err);
    }

    _state = HandlerState::FINALIZE;
    co_return;
  } catch (Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("11928", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught exception in " << name() << ": "
        << ex.what();
#endif
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("fdcbb", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught velocypack exception in " << name() << ": "
        << ex.what();
#endif
    bool const isParseError =
        (ex.errorCode() == arangodb::velocypack::Exception::ParseError ||
         ex.errorCode() ==
             arangodb::velocypack::Exception::UnexpectedControlCharacter);
    Exception err(
        isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL,
        std::string("VPack error: ") + ex.what());
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("5c9f5", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught memory exception in " << name() << ": "
        << ex.what();
#endif
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what());
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("252e9", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught exception in " << name() << ": "
        << ex.what();
#endif
    Exception err(TRI_ERROR_INTERNAL, ex.what());
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("f729c", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught unknown exception in " << name();
#endif
    Exception err(TRI_ERROR_INTERNAL);
    handleError(err);
  }

  _state = HandlerState::FAILED;
}
