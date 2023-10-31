////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RemoteExecutor.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlExecuteResult.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ProfileLevel.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RestAqlHandler.h"
#include "Aql/SharedQueryState.h"
#include "Aql/SkipResult.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/CommonDefines.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

using arangodb::basics::VelocyPackHelper;

namespace {
/// @brief timeout
constexpr std::chrono::seconds kDefaultTimeOutSecs(3600);
}  // namespace

ExecutionBlockImpl<RemoteExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, RemoteNode const* node,
    RegisterInfos&& registerInfos, std::string const& server,
    std::string const& distributeId, std::string const& queryId)
    : ExecutionBlock(engine, node),
      _registerInfos(std::move(registerInfos)),
      _query(engine->getQuery()),
      _server(server),
      _distributeId(distributeId),
      _queryId(queryId),
      _lastError(TRI_ERROR_NO_ERROR),
      _isResponsibleForInitializeCursor(
          node->isResponsibleForInitializeCursor()),
      _requestInFlight(false),
      _lastTicket(0) {
  TRI_ASSERT(!queryId.empty());
  TRI_ASSERT((arangodb::ServerState::instance()->isCoordinator() &&
              distributeId.empty()) ||
             (!arangodb::ServerState::instance()->isCoordinator() &&
              !distributeId.empty()));
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<
    RemoteExecutor>::initializeCursor(InputAqlItemRow const& input) {
  // For every call we simply forward via HTTP

  if (!_isResponsibleForInitializeCursor) {
    // do nothing...
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (!input.isInitialized()) {
    // we simply ignore the initialCursor request, as the remote side
    // will initialize the cursor lazily
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  std::unique_lock<std::mutex> guard(_communicationMutex);

  if (_requestInFlight) {
    return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
  }

  if (_lastResponse != nullptr) {
    // We have an open result still.
    auto response = std::move(_lastResponse);

    // Result is the response which is an object containing the ErrorCode
    auto errorNumber = TRI_ERROR_INTERNAL;  // default error code
    VPackSlice slice = response->slice();
    VPackSlice errorSlice = slice.get(StaticStrings::ErrorNum);
    if (!errorSlice.isNumber()) {
      errorSlice = slice.get(StaticStrings::Code);
    }
    if (errorSlice.isNumber()) {
      errorNumber = ErrorCode{errorSlice.getNumericValue<int>()};
    }

    errorSlice = slice.get(StaticStrings::ErrorMessage);
    if (errorSlice.isString()) {
      return {ExecutionState::DONE, {errorNumber, errorSlice.copyString()}};
    }
    return {ExecutionState::DONE, errorNumber};
  } else if (_lastError.fail()) {
    return {ExecutionState::DONE, std::move(_lastError)};
  }

  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer, &options);
  builder.openObject(/*unindexed*/ true);

  // always false. currently still needed in mixed environments of
  // 3.11 and higher. can be removed for 3.13.
  builder.add("done", VPackValue(false));

  builder.add(StaticStrings::Code, VPackValue(TRI_ERROR_NO_ERROR));
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(VPackValue("items"));
  builder.openObject(/*unindexed*/ true);
  input.toVelocyPack(&_engine->getQuery().vpackOptions(), builder);
  builder.close();

  builder.close();

  traceInitializeCursorRequest(builder.slice());

  auto res = sendAsyncRequest(fuerte::RestVerb::Put,
                              "/_api/aql/initializeCursor", std::move(buffer));
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

auto ExecutionBlockImpl<RemoteExecutor>::execute(AqlCallStack const& stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool old = false;
  TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
  TRI_ASSERT(_isBlockInUse);

  auto guard = scopeGuard([&]() noexcept {
    bool old = true;
    TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
    TRI_ASSERT(!_isBlockInUse);
  });
#endif

  traceExecuteBegin(stack);
  auto res = executeWithoutTrace(stack);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& [state, skipped, block] = res;
  if (block != nullptr) {
    block->validateShadowRowConsistency();
  }
#endif
  if (_profileLevel >= ProfileLevel::Blocks) {
    traceExecuteEnd(res, absl::StrCat(server(), ":", distributeId()));
  }
  return res;
}

auto ExecutionBlockImpl<RemoteExecutor>::executeWithoutTrace(
    AqlCallStack const& stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  // silence tests -- we need to introduce new failure tests for fetchers
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  std::unique_lock<std::mutex> guard(_communicationMutex);

  if (_requestInFlight) {
    // Already sent a shutdown request, but haven't got an answer yet.
    return {ExecutionState::WAITING, SkipResult{}, nullptr};
  }

  // For every call we simply forward via HTTP
  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = std::move(_lastError);
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());
    // We do not have an error but a result, all is good
    // We have an open result still.
    auto response = std::move(_lastResponse);
    // Result is the response which will be a serialized AqlItemBlock

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice responseBody = response->slice();

    auto result = deserializeExecuteCallResultBody(responseBody);

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result.result());
    }

    return result->asTuple();
  }

  // We need to send a request here
  auto buffer = serializeExecuteCallBody(stack);
  this->traceExecuteRequest(VPackSlice(buffer.data()), stack);

  auto res =
      sendAsyncRequest(fuerte::RestVerb::Put, RestAqlHandler::Route::execute(),
                       std::move(buffer));

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, SkipResult{}, nullptr};
}

auto ExecutionBlockImpl<RemoteExecutor>::deserializeExecuteCallResultBody(
    VPackSlice slice) const -> ResultT<AqlExecuteResult> {
  // Errors should have been caught earlier
  TRI_ASSERT(
      TRI_ERROR_NO_ERROR ==
      (VelocyPackHelper::getNumericValue<ErrorCode, ErrorCode::ValueType>(
          slice, StaticStrings::Code, TRI_ERROR_INTERNAL)));

  if (ADB_UNLIKELY(!slice.isObject())) {
    return Result{
        TRI_ERROR_TYPE_ERROR,
        absl::StrCat("When parsing execute result: expected object, got ",
                     slice.typeName())};
  }

  if (auto value = slice.get(StaticStrings::AqlRemoteResult); !value.isNone()) {
    return AqlExecuteResult::fromVelocyPack(value, _engine->itemBlockManager());
  }

  return Result{TRI_ERROR_TYPE_ERROR,
                "When parsing execute result: field result missing"};
}

auto ExecutionBlockImpl<RemoteExecutor>::serializeExecuteCallBody(
    AqlCallStack const& callStack) const -> VPackBuffer<uint8_t> {
  VPackBuffer<uint8_t> buffer;
  {
    VPackBuilder builder(buffer);
    builder.openObject();
    builder.add(VPackValue(StaticStrings::AqlRemoteCallStack));
    callStack.toVelocyPack(builder);
    builder.close();
  }
  return buffer;
}

namespace {
Result handleErrorResponse(network::EndpointSpec const& spec, fuerte::Error err,
                           fuerte::Response* response) {
  TRI_ASSERT(err != fuerte::Error::NoError || response->statusCode() >= 400);

  constexpr std::string_view prefix("Error message received from ");
  std::string msg;
  // avoid a few reallocations for building the error message string
  msg.reserve(128);
  msg.append(prefix);
  if (!spec.shardId.empty()) {
    msg.append("shard '").append(spec.shardId).append("' on ");
  }
  msg.append("cluster node '").append(spec.serverId).append("': ");

  auto res = TRI_ERROR_INTERNAL;
  if (err != fuerte::Error::NoError) {
    res = network::fuerteToArangoErrorCode(err);
    msg.append(TRI_errno_string(res));
  } else {
    VPackSlice slice = response->slice();
    if (slice.isObject()) {
      VPackSlice errSlice = slice.get(StaticStrings::Error);
      if (errSlice.isTrue()) {
        res =
            VelocyPackHelper::getNumericValue<ErrorCode, ErrorCode::ValueType>(
                slice, StaticStrings::ErrorNum, res);
        std::string_view ref = VelocyPackHelper::getStringView(
            slice, StaticStrings::ErrorMessage,
            std::string_view("(no valid error in response)"));

        if (ref.starts_with(prefix)) {
          // error message already starts with "Error message received from ".
          // this can happen if an error is received by a DB server sending a
          // request to a coordinator, and then sending the coordinator's
          // error back to the original calling snippet on the coordinator.
          // in this case, we simply keep the original (first) error.
          msg.clear();
        }

        msg.append(ref.data(), ref.size());
      }
    }
  }

  return Result(res, std::move(msg));
}
}  // namespace

Result ExecutionBlockImpl<RemoteExecutor>::sendAsyncRequest(
    fuerte::RestVerb type, std::string const& urlPart,
    VPackBuffer<uint8_t>&& body) {
  NetworkFeature const& nf =
      _engine->getQuery().vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  arangodb::network::EndpointSpec spec;
  auto res = network::resolveDestination(nf, _server, spec);
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res);
  }
  TRI_ASSERT(!spec.endpoint.empty());

  auto req = fuerte::createRequest(type, fuerte::ContentType::VPack);
  req->header.database = _query.vocbase().name();
  req->header.path = absl::StrCat(urlPart, "/", _queryId);
  req->addVPack(std::move(body));

  // Later, we probably want to set these sensibly:
  req->timeout(kDefaultTimeOutSecs);
  if (!_distributeId.empty()) {
    req->header.addMeta(StaticStrings::AqlShardIdHeader, _distributeId);
  }

  network::addSourceHeader(nullptr, *req);

  LOG_TOPIC("2713c", DEBUG, Logger::COMMUNICATION)
      << "request to '" << _server << "' '" << fuerte::to_string(type) << " "
      << req->header.path << "'";

  bool isFromPool;
  network::ConnectionPtr conn =
      pool->leaseConnection(spec.endpoint, isFromPool);

  _requestInFlight = true;
  auto ticket = generateRequestTicket();
  TRI_IF_FAILURE("RemoteExecutor::impatienceTimeout") {
    // Vastly lower the request timeout. This should guarantee
    // a network timeout triggered and not continue with the query.
    req->timeout(std::chrono::seconds(2));
  }

  conn->sendRequest(
      std::move(req),
      [this, ticket, spec, sqs = _engine->sharedState()](
          fuerte::Error err, std::unique_ptr<fuerte::Request> req,
          std::unique_ptr<fuerte::Response> res) {
        // `this` is only valid as long as sharedState is valid.
        // So we must execute this under sharedState's mutex.
        sqs->executeAndWakeup([&] {
          std::lock_guard<std::mutex> guard(_communicationMutex);
          if (_lastTicket == ticket) {
            if (err != fuerte::Error::NoError || res->statusCode() >= 400) {
              _lastError = handleErrorResponse(spec, err, res.get());
            } else {
              _lastResponse = std::move(res);
            }
            _requestInFlight = false;
            return true;
          }
          return false;
        });
      });

  _engine->getQuery().incHttpRequests(unsigned(1));

  return {};
}

void ExecutionBlockImpl<RemoteExecutor>::traceExecuteRequest(
    VPackSlice slice, AqlCallStack const& callStack) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires it
    traceRequest("execute", slice,
                 absl::StrCat("callStack=", callStack.toString()));
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceInitializeCursorRequest(
    VPackSlice slice) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires it
    traceRequest("initializeCursor", slice, "");
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceRequest(std::string_view rpc,
                                                      VPackSlice slice,
                                                      std::string_view args) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    auto queryId = _engine->getQuery().id();
    LOG_TOPIC("92c71", INFO, Logger::QUERIES)
        << "[query#" << queryId << "] remote request sent: " << rpc
        << (args.empty() ? "" : " ") << args << " registryId=" << _queryId;
    LOG_TOPIC_IF("e0ae6", INFO, Logger::QUERIES,
                 _profileLevel == ProfileLevel::TraceTwo)
        << "[query#" << queryId << "] data: " << slice.toJson();
  }
}

unsigned ExecutionBlockImpl<RemoteExecutor>::generateRequestTicket() {
  // Assumes that _communicationMutex is locked in the caller
  ++_lastTicket;
  _lastError.reset();
  _lastResponse.reset();

  return _lastTicket;
}
