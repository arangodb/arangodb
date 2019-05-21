////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/WakeupQueryCallback.h"
#include "Basics/MutexLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"

#include <lib/Rest/CommonDefines.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using arangodb::basics::VelocyPackHelper;

/// @brief timeout
double const ExecutionBlockImpl<RemoteExecutor>::defaultTimeOut = 3600.0;

ExecutionBlockImpl<RemoteExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, RemoteNode const* node, ExecutorInfos&& infos,
    std::string const& server, std::string const& ownName, std::string const& queryId)
    : ExecutionBlock(engine, node),
      _infos(std::move(infos)),
      _query(*engine->getQuery()),
      _server(server),
      _ownName(ownName),
      _queryId(queryId),
      _isResponsibleForInitializeCursor(node->isResponsibleForInitializeCursor()),
      _lastResponse(nullptr),
      _lastError(TRI_ERROR_NO_ERROR),
      _lastTicketId(0),
      _hasTriggeredShutdown(false) {
  TRI_ASSERT(!queryId.empty());
  TRI_ASSERT((arangodb::ServerState::instance()->isCoordinator() && ownName.empty()) ||
             (!arangodb::ServerState::instance()->isCoordinator() && !ownName.empty()));
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<RemoteExecutor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
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

  // For every call we simply forward via HTTP
  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());
    // We do not have an error but a result, all is good
    // We have an open result still.
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();
    // Result is the response which will be a serialized AqlItemBlock

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice responseBody = responseBodyBuilder->slice();

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
  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/getSome/", bodyString);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, nullptr};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<RemoteExecutor>::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  auto result = skipSomeWithoutTrace(atMost);
  return traceSkipSomeEnd(result.first, result.second);
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<RemoteExecutor>::skipSomeWithoutTrace(size_t atMost) {
  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());

    // We have an open result still.
    // Result is the response which will be a serialized AqlItemBlock
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = responseBodyBuilder->slice();

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

  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/skipSome/", bodyString);
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

  if (_lastResponse != nullptr || _lastError.fail()) {
    // We have an open result still.
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // Result is the response which is an object containing the ErrorCode
    VPackSlice slice = responseBodyBuilder->slice();
    if (slice.hasKey("code")) {
      return {ExecutionState::DONE, slice.get("code").getNumericValue<int>()};
    }
    return {ExecutionState::DONE, TRI_ERROR_INTERNAL};
  }

  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder builder(&options);
  builder.openObject();

  // Backwards Compatibility 3.3
  // NOTE: Removing this breaks tests in current devel - is this really for
  // bc only?
  builder.add("exhausted", VPackValue(false));
  // Used in 3.4.0 onwards
  builder.add("done", VPackValue(false));
  builder.add("error", VPackValue(false));
  // NOTE API change. Before all items have been send.
  // Now only the one output row is send.
  builder.add("pos", VPackValue(0));
  builder.add(VPackValue("items"));
  builder.openObject();
  input.toVelocyPack(_engine->getQuery()->trx(), builder);
  builder.close();

  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT,
                              "/_api/aql/initializeCursor/", bodyString);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionBlockImpl<RemoteExecutor>::shutdown(int errorCode) {

  if (!_isResponsibleForInitializeCursor) {
    // do nothing...
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (!_hasTriggeredShutdown) {
    // Make sure to cover against the race that the request
    // in flight is not overtaking in the drop phase here.
    // After this lock is released even a response
    // will be discarded in the handle response code
    MUTEX_LOCKER(locker, _communicationMutex);
    if (_lastTicketId != 0) {
      auto cc = ClusterComm::instance();
      if (cc == nullptr) {
        // nullptr only happens on controlled shutdown
        return {ExecutionState::DONE, TRI_ERROR_SHUTTING_DOWN};
      }
      cc->drop(0, _lastTicketId, "");
    }
    _lastTicketId = 0;
    _lastError.reset(TRI_ERROR_NO_ERROR);
    _lastResponse.reset();
    _hasTriggeredShutdown = true;
  }

  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();

    if (res.is(TRI_ERROR_QUERY_NOT_FOUND)) {
      // Ignore query not found errors, they should do no harm.
      // However, as only the snippet with _isResponsibleForInitializeCursor
      // should now call shutdown, this should not usually happen, so emit a
      // warning.
      LOG_TOPIC("8d035", WARN, Logger::AQL)
          << "During AQL query shutdown: "
          << "Query ID " << _queryId << " not found on " << _server;
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    }

    // we were called with an error and need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());

    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = responseBodyBuilder->slice();
    if (slice.isObject()) {
      if (slice.hasKey("stats")) {
        ExecutionStats newStats(slice.get("stats"));
        _engine->_stats.add(newStats);
      }

      // read "warnings" attribute if present and add it to our query
      VPackSlice warnings = slice.get("warnings");
      if (warnings.isArray()) {
        auto query = _engine->getQuery();
        for (auto const& it : VPackArrayIterator(warnings)) {
          if (it.isObject()) {
            VPackSlice code = it.get("code");
            VPackSlice message = it.get("message");
            if (code.isNumber() && message.isString()) {
              query->registerWarning(code.getNumericValue<int>(),
                                     message.copyString().c_str());
            }
          }
        }
      }
      if (slice.hasKey("code")) {
        return {ExecutionState::DONE, slice.get("code").getNumericValue<int>()};
      }
    }

    return {ExecutionState::DONE, TRI_ERROR_INTERNAL};
  }

  // For every call we simply forward via HTTP
  VPackBuilder bodyBuilder;
  bodyBuilder.openObject();
  bodyBuilder.add("code", VPackValue(errorCode));
  bodyBuilder.close();

  auto bodyString = std::make_shared<std::string const>(bodyBuilder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/shutdown/", bodyString);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

Result ExecutionBlockImpl<RemoteExecutor>::sendAsyncRequest(
    arangodb::rest::RequestType type, std::string const& urlPart,
    std::shared_ptr<std::string const> body) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  // Later, we probably want to set these sensibly:
  CoordTransactionID const coordTransactionId = TRI_NewTickServer();
  std::unordered_map<std::string, std::string> headers;
  if (!_ownName.empty()) {
    headers.emplace("Shard-Id", _ownName);
  }

  std::string url = std::string("/_db/") +
                    arangodb::basics::StringUtils::urlEncode(
                        _engine->getQuery()->trx()->vocbase().name()) +
                    urlPart + _queryId;

  ++_engine->_stats.requests;
  std::shared_ptr<ClusterCommCallback> callback =
      std::make_shared<WakeupQueryCallback>(this, _engine->getQuery());

  // Make sure to cover against the race that this
  // Request is fullfilled before the register has taken place
  // @note the only reason for not using recursive mutext always is due to the
  //       concern that there might be recursive calls in production
  #ifdef ARANGODB_USE_GOOGLE_TESTS
    RECURSIVE_MUTEX_LOCKER(_communicationMutex, _communicationMutexOwner);
  #else
    MUTEX_LOCKER(locker, _communicationMutex);
  #endif

  // We can only track one request at a time.
  // So assert there is no other request in flight!
  TRI_ASSERT(_lastTicketId == 0);
  _lastTicketId =
      cc->asyncRequest(coordTransactionId, _server, type, url, std::move(body),
                       headers, callback, defaultTimeOut, true);

  return {TRI_ERROR_NO_ERROR};
}

bool ExecutionBlockImpl<RemoteExecutor>::handleAsyncResult(ClusterCommResult* result) {
  // So we cannot have the response being produced while sending the request.
  // Make sure to cover against the race that this
  // Request is fullfilled before the register has taken place
  // @note the only reason for not using recursive mutext always is due to the
  //       concern that there might be recursive calls in production
  #ifdef ARANGODB_USE_GOOGLE_TESTS
    RECURSIVE_MUTEX_LOCKER(_communicationMutex, _communicationMutexOwner);
  #else
    MUTEX_LOCKER(locker, _communicationMutex);
  #endif

  if (_lastTicketId == result->operationID) {
    // TODO Handle exceptions thrown while we are in this code
    // Query will not be woken up again.
    _lastError = handleCommErrors(result);
    if (_lastError.ok()) {
      _lastResponse = result->result;
    }
    _lastTicketId = 0;
  }
  return true;
}

arangodb::Result ExecutionBlockImpl<RemoteExecutor>::handleCommErrors(ClusterCommResult* res) const {
  if (res->status == CL_COMM_TIMEOUT || res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    return {res->getErrorCode(), res->stringifyErrorMessage()};
  }
  if (res->status == CL_COMM_ERROR) {
    std::string errorMessage;
    auto const& shardID = res->shardID;

    if (shardID.empty()) {
      errorMessage = std::string("Error message received from cluster node '") +
                     std::string(res->serverID) + std::string("': ");
    } else {
      errorMessage = std::string("Error message received from shard '") +
                     std::string(shardID) + std::string("' on cluster node '") +
                     std::string(res->serverID) + std::string("': ");
    }

    int errorNum = TRI_ERROR_INTERNAL;
    if (res->result != nullptr) {
      errorNum = TRI_ERROR_NO_ERROR;
      arangodb::basics::StringBuffer const& responseBodyBuf(res->result->getBody());
      std::shared_ptr<VPackBuilder> builder =
          VPackParser::fromJson(responseBodyBuf.c_str(), responseBodyBuf.length());
      VPackSlice slice = builder->slice();

      if (!slice.hasKey(StaticStrings::Error) ||
          slice.get(StaticStrings::Error).getBoolean()) {
        errorNum = TRI_ERROR_INTERNAL;
      }

      if (slice.isObject()) {
        VPackSlice v = slice.get(StaticStrings::ErrorNum);
        if (v.isNumber()) {
          if (v.getNumericValue<int>() != TRI_ERROR_NO_ERROR) {
            /* if we've got an error num, error has to be true. */
            TRI_ASSERT(errorNum == TRI_ERROR_INTERNAL);
            errorNum = v.getNumericValue<int>();
          }
        }

        v = slice.get(StaticStrings::ErrorMessage);
        if (v.isString()) {
          errorMessage += v.copyString();
        } else {
          errorMessage += std::string("(no valid error in response)");
        }
      }
    }
    // In this case a proper HTTP error was reported by the DBserver,
    if (errorNum > 0 && !errorMessage.empty()) {
      return {errorNum, errorMessage};
    }

    // default error
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION};
  }

  TRI_ASSERT(res->status == CL_COMM_SENT);

  return {TRI_ERROR_NO_ERROR};
}

/**
 * @brief Steal the last returned body. Will throw an error if
 *        there has been an error of any kind, e.g. communication
 *        or error created by remote server.
 *        Will reset the lastResponse, so after this call we are
 *        ready to send a new request.
 *
 * @return A shared_ptr containing the remote response.
 */
std::shared_ptr<VPackBuilder> ExecutionBlockImpl<RemoteExecutor>::stealResultBody() {
  // NOTE: This cannot participate in the race in communication.
  // This will not be called after the MUTEX to send was released.
  // It can only be called by the next getSome call.
  // This getSome however is locked by the QueryRegistery several layers above
  if (!_lastError.ok()) {
    THROW_ARANGO_EXCEPTION(_lastError);
  }
  // We have an open result still.
  // Result is the response which is an object containing the ErrorCode
  std::shared_ptr<VPackBuilder> responseBodyBuilder = _lastResponse->getBodyVelocyPack();
  _lastResponse.reset();
  return responseBodyBuilder;
}
