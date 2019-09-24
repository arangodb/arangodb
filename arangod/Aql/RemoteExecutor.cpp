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
#include "Basics/MutexLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/CommonDefines.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using arangodb::basics::VelocyPackHelper;

namespace {
/// @brief timeout
  int constexpr kDefaultTimeOutSecs = 3600.0;
}

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
      _lastError(TRI_ERROR_NO_ERROR),
      _hasTriggeredShutdown(false),
      _lastTicket(0) {
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
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(fuerte::RestVerb::Put, "/_api/aql/getSome/", std::move(buffer));
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
  VPackBuilder builder(buffer);
  builder.openObject(/*unindexed*/true);
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(fuerte::RestVerb::Put, "/_api/aql/skipSome/", std::move(buffer));
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
    auto response = std::move(_lastResponse);

    // Result is the response which is an object containing the ErrorCode
    VPackSlice slice = response->slice();
    if (slice.hasKey("code")) {
      return {ExecutionState::DONE, slice.get("code").getNumericValue<int>()};
    }
    return {ExecutionState::DONE, TRI_ERROR_INTERNAL};
  }

  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer, &options);
  builder.openObject(/*unindexed*/true);

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
  builder.openObject(/*unindexed*/true);
  input.toVelocyPack(_engine->getQuery()->trx(), builder);
  builder.close();

  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(fuerte::RestVerb::Put,
                              "/_api/aql/initializeCursor/", std::move(buffer));
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
//    MUTEX_LOCKER(locker, _communicationMutex);
//    if (_lastTicketId != 0) {
//      auto cc = ClusterComm::instance();
//      if (cc == nullptr) {
//        // nullptr only happens on controlled shutdown
//        return {ExecutionState::DONE, TRI_ERROR_SHUTTING_DOWN};
//      }
//      cc->drop(0, _lastTicketId, "");
//    }
//    _lastTicketId = 0;
    _lastTicket.store(0);
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
    
    auto response = std::move(_lastResponse);

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = response->slice();
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_didSendShutdownRequest == false);
  _didSendShutdownRequest = true;
#endif
  // For every call we simply forward via HTTP
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject(/*unindexed*/true);
  builder.add("code", VPackValue(errorCode));
  builder.close();

  auto res = sendAsyncRequest(fuerte::RestVerb::Put, "/_api/aql/shutdown/", std::move(buffer));
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

namespace {
Result handleErrorResponse(network::EndpointSpec const& spec,
                           fuerte::Error err,
                           fuerte::Response* response) {
  TRI_ASSERT(err != fuerte::Error::NoError ||
             response->statusCode() >= 400);
  if (err != fuerte::Error::NoError) {
    return Result(network::fuerteToArangoErrorCode(err));
  }
  
  std::string msg;
  if (spec.shardId.empty()) {
    msg.append("Error message received from cluster node '")
       .append(spec.serverId).append("': ");
  } else {
    msg.append("Error message received from shard '")
       .append(spec.shardId).append("' on cluster node '")
       .append(spec.serverId).append("': ");
  }
  
  int res = TRI_ERROR_INTERNAL;
  VPackSlice slice = response->slice();
  if (slice.isObject()) {
    VPackSlice err = slice.get(StaticStrings::Error);
    if (err.isBool() && err.getBool()) {
      res = VelocyPackHelper::readNumericValue(slice, StaticStrings::ErrorNum, res);
      VPackStringRef ref = VelocyPackHelper::getStringRef(slice, StaticStrings::ErrorMessage,
                                                          "(no valid error in response)");
      msg.append(ref.data(), ref.size());
    }
  }
  
  return Result(res, std::move(msg));
}
}

Result ExecutionBlockImpl<RemoteExecutor>::sendAsyncRequest(
    fuerte::RestVerb type, std::string const& urlPart,
    VPackBuffer<uint8_t> body) {

  network::ConnectionPool* pool = NetworkFeature::pool();
  if (!pool) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }
  
  std::string url = std::string("/_db/") +
                    arangodb::basics::StringUtils::urlEncode(
                        _engine->getQuery()->trx()->vocbase().name()) +
                    urlPart + _queryId;
  
  arangodb::network::EndpointSpec spec;
  int res = network::resolveDestination(_server, spec);
  if (res != TRI_ERROR_NO_ERROR) {  // FIXME return an error  ?!
    return Result(res);
  }
  TRI_ASSERT(!spec.endpoint.empty());
  
  auto req = fuerte::createRequest(type, url, {}, std::move(body));
  // Later, we probably want to set these sensibly:
  req->timeout(std::chrono::seconds(kDefaultTimeOutSecs));
  if (!_ownName.empty()) {
    req->header.addMeta("Shard-Id", _ownName);
  }
  
  network::ConnectionPool::Ref ref = pool->leaseConnection(spec.endpoint);
  
  unsigned ticket = _lastTicket.fetch_add(1) + 1;
  std::shared_ptr<fuerte::Connection> conn = ref.connection();
  conn->sendRequest(std::move(req),
                    [=, ref(std::move(ref))](fuerte::Error err,
                                             std::unique_ptr<fuerte::Request>,
                                             std::unique_ptr<fuerte::Response> res) {
    _query.sharedState()->execute([&] { // notifies outside
      if (_lastTicket.load() == ticket) {
        if (err != fuerte::Error::NoError || res->statusCode() >= 400) {
          _lastError = handleErrorResponse(spec, err, res.get());
        } else {
          _lastResponse = std::move(res);
        }
      }
    });
  });
  
  ++_engine->_stats.requests;
  
//  std::shared_ptr<ClusterCommCallback> callback =
//      std::make_shared<WakeupQueryCallback>(this, _engine->getQuery());
//
//  // We can only track one request at a time.
//  // So assert there is no other request in flight!
//  TRI_ASSERT(_lastTicketId == 0);
//  _lastTicketId =
//      cc->asyncRequest(coordTransactionId, _server, type, url, std::move(body),
//                       headers, callback, defaultTimeOut, true);

  return {TRI_ERROR_NO_ERROR};
}
//
//bool ExecutionBlockImpl<RemoteExecutor>::handleAsyncResult(ClusterCommResult* result) {
//  // So we cannot have the response being produced while sending the request.
//  // Make sure to cover against the race that this
//  // Request is fullfilled before the register has taken place
//  // @note the only reason for not using recursive mutext always is due to the
//  //       concern that there might be recursive calls in production
//  #ifdef ARANGODB_USE_GOOGLE_TESTS
//    RECURSIVE_MUTEX_LOCKER(_communicationMutex, _communicationMutexOwner);
//  #else
//    MUTEX_LOCKER(locker, _communicationMutex);
//  #endif
//
//  if (_lastTicketId == result->operationID) {
//    // TODO Handle exceptions thrown while we are in this code
//    // Query will not be woken up again.
//    _lastError = handleCommErrors(result);
//    if (_lastError.ok()) {
//      _lastResponse = result->result;
//    }
//    _lastTicketId = 0;
//  }
//  return true;
//}

#if 0
arangodb::Result ExecutionBlockImpl<RemoteExecutor>::handleCommErrors(fuerte::Response const&) const {
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
#endif
