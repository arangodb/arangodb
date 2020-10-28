////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/QueryContext.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RestAqlHandler.h"
#include "Aql/SharedQueryState.h"
#include "Aql/SkipResult.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
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
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using arangodb::basics::VelocyPackHelper;

namespace {
/// @brief timeout
constexpr std::chrono::seconds kDefaultTimeOutSecs(3600);
}  // namespace

ExecutionBlockImpl<RemoteExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, RemoteNode const* node, RegisterInfos&& registerInfos,
    std::string const& server, std::string const& distributeId,
    std::string const& queryId)
    : ExecutionBlock(engine, node),
      _registerInfos(std::move(registerInfos)),
      _query(engine->getQuery()),
      _server(server),
      _distributeId(distributeId),
      _queryId(queryId),
      _isResponsibleForInitializeCursor(node->isResponsibleForInitializeCursor()),
      _lastError(TRI_ERROR_NO_ERROR),
      _lastTicket(0),
      _requestInFlight(false) {
  TRI_ASSERT(!queryId.empty());
  TRI_ASSERT((arangodb::ServerState::instance()->isCoordinator() && distributeId.empty()) ||
             (!arangodb::ServerState::instance()->isCoordinator() && !distributeId.empty()));
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<RemoteExecutor>::getSomeWithoutTrace(size_t atMost) {
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
    // Already sent a getSome request, but haven't got an answer yet.
    return {ExecutionState::WAITING, nullptr};
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

    ExecutionState state = ExecutionState::HASMORE;
    if (VelocyPackHelper::getBooleanValue(responseBody, "done", true)) {
      state = ExecutionState::DONE;
    }
    if (responseBody.hasKey("data")) {
      SharedAqlItemBlockPtr r =
          _engine->itemBlockManager().requestAndInitBlock(responseBody);

      return {state, std::move(r)};
    }
    return {ExecutionState::DONE, nullptr};
  }

  // We need to send a request here
  VPackBuffer<uint8_t> buffer;
  {
    VPackBuilder builder(buffer);
    builder.openObject();
    builder.add("atMost", VPackValue(atMost));
    builder.close();
    traceGetSomeRequest(builder.slice(), atMost);
  }

  auto res = sendAsyncRequest(fuerte::RestVerb::Put, "/_api/aql/getSome", std::move(buffer));

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, nullptr};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<RemoteExecutor>::skipSomeWithoutTrace(size_t atMost) {
  std::unique_lock<std::mutex> guard(_communicationMutex);

  if (_requestInFlight) {
    // Already sent a shutdown request, but haven't got an answer yet.
    return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
  }

  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());
    TRI_ASSERT(_requestInFlight == false);

    // We have an open result still.
    // Result is the response which will be a serialized AqlItemBlock
    auto response = std::move(_lastResponse);

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = response->slice();

    if (!slice.hasKey(StaticStrings::Error) || slice.get(StaticStrings::Error).getBoolean()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
    }
    size_t skipped = 0;
    VPackSlice s = slice.get("skipped");
    if (s.isNumber()) {
      auto value = s.getNumericValue<int64_t>();
      if (value < 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "skipped cannot be negative");
      }
      skipped = s.getNumericValue<size_t>();
    }

    // TODO Check if we can get better with HASMORE/DONE
    if (skipped == 0) {
      return {ExecutionState::DONE, skipped};
    }
    return {ExecutionState::HASMORE, skipped};
  }

  // For every call we simply forward via HTTP

  VPackBuffer<uint8_t> buffer;
  {
    VPackBuilder builder(buffer);
    builder.openObject(/*unindexed*/ true);
    builder.add("atMost", VPackValue(atMost));
    builder.close();
    traceSkipSomeRequest(builder.slice(), atMost);
  }
  auto res = sendAsyncRequest(fuerte::RestVerb::Put, "/_api/aql/skipSome",
                              std::move(buffer));

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, 0};
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<RemoteExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
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
    int errorNumber = TRI_ERROR_INTERNAL;  // default error code
    VPackSlice slice = response->slice();
    VPackSlice errorSlice = slice.get(StaticStrings::ErrorNum);
    if (!errorSlice.isNumber()) {
      errorSlice = slice.get(StaticStrings::Code);
    }
    if (errorSlice.isNumber()) {
      errorNumber = errorSlice.getNumericValue<int>();
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

  // Used from 3.4.0 onwards:
  builder.add("done", VPackValue(false));

  builder.add(StaticStrings::Code, VPackValue(TRI_ERROR_NO_ERROR));
  builder.add(StaticStrings::Error, VPackValue(false));
  // NOTE API change. Before all items have been send.
  // Now only the one output row is send.
  builder.add("pos", VPackValue(0));
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

auto ExecutionBlockImpl<RemoteExecutor>::execute(AqlCallStack stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  traceExecuteBegin(stack);
  auto res = executeWithoutTrace(stack);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& [state, skipped, block] = res;
  if (block != nullptr) {
    block->validateShadowRowConsistency();
  }
#endif
  traceExecuteEnd(res, server() + ":" + distributeId());
  return res;
}

auto ExecutionBlockImpl<RemoteExecutor>::executeWithoutTrace(AqlCallStack stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  return executeViaNewApi(stack);
}

auto ExecutionBlockImpl<RemoteExecutor>::executeViaNewApi(AqlCallStack callStack)
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
  auto buffer = serializeExecuteCallBody(callStack);
  this->traceExecuteRequest(VPackSlice(buffer.data()), callStack);

  auto res = sendAsyncRequest(fuerte::RestVerb::Put,
                              RestAqlHandler::Route::execute(), std::move(buffer));

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, SkipResult{}, nullptr};
}

auto ExecutionBlockImpl<RemoteExecutor>::deserializeExecuteCallResultBody(VPackSlice const slice) const
    -> ResultT<AqlExecuteResult> {
  // Errors should have been caught earlier
  TRI_ASSERT(TRI_ERROR_NO_ERROR ==
             VelocyPackHelper::getNumericValue<int>(slice, StaticStrings::Code, -1));

  if (ADB_UNLIKELY(!slice.isObject())) {
    using namespace std::string_literals;
    return Result{TRI_ERROR_TYPE_ERROR,
                  "When parsing execute result: expected object, got "s + slice.typeName()};
  }

  if (auto value = slice.get(StaticStrings::AqlRemoteResult); !value.isNone()) {
    return AqlExecuteResult::fromVelocyPack(value, _engine->itemBlockManager());
  }

  return Result{TRI_ERROR_TYPE_ERROR,
                "When parsing execute result: field result missing"};
}

auto ExecutionBlockImpl<RemoteExecutor>::serializeExecuteCallBody(AqlCallStack const& callStack) const
    -> VPackBuffer<uint8_t> {
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

  std::string msg;
  if (spec.shardId.empty()) {
    msg.append("Error message received from cluster node '")
        .append(spec.serverId)
        .append("': ");
  } else {
    msg.append("Error message received from shard '")
        .append(spec.shardId)
        .append("' on cluster node '")
        .append(spec.serverId)
        .append("': ");
  }

  int res = TRI_ERROR_INTERNAL;
  if (err != fuerte::Error::NoError) {
    res = network::fuerteToArangoErrorCode(err);
    msg.append(TRI_errno_string(res));
  } else {
    VPackSlice slice = response->slice();
    if (slice.isObject()) {
      VPackSlice errSlice = slice.get(StaticStrings::Error);
      if (errSlice.isBool() && errSlice.getBool()) {
        res = VelocyPackHelper::getNumericValue(slice, StaticStrings::ErrorNum, res);
        VPackStringRef ref =
            VelocyPackHelper::getStringRef(slice, StaticStrings::ErrorMessage,
                                           VPackStringRef(
                                               "(no valid error in response)"));
        msg.append(ref.data(), ref.size());
      }
    }
  }

  return Result(res, std::move(msg));
}
}  // namespace

Result ExecutionBlockImpl<RemoteExecutor>::sendAsyncRequest(fuerte::RestVerb type,
                                                            std::string const& urlPart,
                                                            VPackBuffer<uint8_t>&& body) {
  NetworkFeature const& nf =
    _engine->getQuery().vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  arangodb::network::EndpointSpec spec;
  int res = network::resolveDestination(nf, _server, spec);
  if (res != TRI_ERROR_NO_ERROR) { 
    return Result(res);
  }
  TRI_ASSERT(!spec.endpoint.empty());

  auto req = fuerte::createRequest(type, fuerte::ContentType::VPack);
  req->header.database = _query.vocbase().name();
  req->header.path = urlPart + "/" + _queryId;
  req->addVPack(std::move(body));

  // Later, we probably want to set these sensibly:
  req->timeout(kDefaultTimeOutSecs);
  if (!_distributeId.empty()) {
    req->header.addMeta(StaticStrings::AqlShardIdHeader, _distributeId);
  }
  network::addSourceHeader(*req);

  LOG_TOPIC("2713c", DEBUG, Logger::COMMUNICATION)
      << "request to '" << _server << "' '" << fuerte::to_string(type) << " "
      << req->header.path << "'";

  network::ConnectionPtr conn = pool->leaseConnection(spec.endpoint);

  _requestInFlight = true;
  auto ticket = generateRequestTicket();
  conn->sendRequest(std::move(req), [this, ticket, spec, sqs = _engine->sharedState()](
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

  return {TRI_ERROR_NO_ERROR};
}

void ExecutionBlockImpl<RemoteExecutor>::traceExecuteRequest(VPackSlice const slice,
                                                             AqlCallStack const& callStack) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires us
    using namespace std::string_literals;
    traceRequest("execute", slice, "callStack="s + callStack.toString());
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceGetSomeRequest(VPackSlice const slice,
                                                             size_t const atMost) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires us
    using namespace std::string_literals;
    traceRequest("getSome", slice, "atMost="s + std::to_string(atMost));
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceSkipSomeRequest(VPackSlice const slice,
                                                              size_t const atMost) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires us
    using namespace std::string_literals;
    traceRequest("skipSome", slice, "atMost="s + std::to_string(atMost));
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceInitializeCursorRequest(VPackSlice const slice) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    // only stringify if profile level requires us
    using namespace std::string_literals;
    traceRequest("initializeCursor", slice, ""s);
  }
}

void ExecutionBlockImpl<RemoteExecutor>::traceRequest(char const* const rpc,
                                                      VPackSlice const slice,
                                                      std::string const& args) {
  if (_profileLevel == ProfileLevel::TraceOne ||
      _profileLevel == ProfileLevel::TraceTwo) {
    auto const queryId = this->_engine->getQuery().id();
    auto const remoteQueryId = _queryId;
    LOG_TOPIC("92c71", INFO, Logger::QUERIES)
        << "[query#" << queryId << "] remote request sent: " << rpc
        << (args.empty() ? "" : " ") << args << " registryId=" << remoteQueryId;
    LOG_TOPIC_IF("e0ae6", INFO, Logger::QUERIES, _profileLevel == ProfileLevel::TraceTwo)
        << "[query#" << queryId << "] data: " << slice.toJson();
  }
}

unsigned ExecutionBlockImpl<RemoteExecutor>::generateRequestTicket() {
  // Assumes that _communicationMutex is locked in the caller
  ++_lastTicket;
  _lastError.reset(TRI_ERROR_NO_ERROR);
  _lastResponse.reset();

  return _lastTicket;
}
