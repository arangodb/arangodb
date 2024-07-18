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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Utils.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/TokenCache.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "VocBase/ticks.h"

#include <fuerte/types.h>

namespace arangodb::network {

futures::Future<ErrorCode> resolveDestination(NetworkFeature const& feature,
                                              DestinationId const& dest,
                                              network::EndpointSpec& spec) {
  // Now look up the actual endpoint:
  if (!feature.server().hasFeature<ClusterFeature>()) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  auto& ci = feature.server().getFeature<ClusterFeature>().clusterInfo();
  return resolveDestination(ci, dest, spec);
}

futures::Future<ErrorCode> resolveDestination(ClusterInfo& ci,
                                              DestinationId const& dest,
                                              network::EndpointSpec& spec) {
  using namespace arangodb;

  if (dest.starts_with("tcp://") || dest.starts_with("ssl://")) {
    spec.endpoint = dest;
    co_return TRI_ERROR_NO_ERROR;  // all good
  }

  if (dest.starts_with("http+tcp://") || dest.starts_with("http+ssl://")) {
    spec.endpoint = dest.substr(5);
    co_return TRI_ERROR_NO_ERROR;
  }

  // This sets result.shardId, result.serverId and result.endpoint,
  // depending on what dest is. Note that if a shardID is given, the
  // responsible server is looked up, if a serverID is given, the endpoint
  // is looked up, both can fail and immediately lead to a CL_COMM_ERROR
  // state.

  if (dest.starts_with("shard:")) {
    spec.shardId = dest.substr(6);
    auto maybeShard = ShardID::shardIdFromString(spec.shardId);
    if (maybeShard.fail()) {
      LOG_TOPIC("005c4", ERR, Logger::COMMUNICATION)
          << "Invalid shard id specified as destination `" << dest
          << "`: " << maybeShard.errorMessage();
      co_return maybeShard.errorNumber();
    }

    auto maybeServer = co_await ci.getLeaderForShard(*maybeShard);
    if (maybeServer.fail()) {
      LOG_TOPIC("60ee8", ERR, Logger::CLUSTER)
          << "cannot find responsible server for shard '" << spec.shardId
          << "': " << maybeServer.errorMessage();
      co_return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }

    spec.serverId = std::move(*maybeServer);
  } else if (dest.starts_with("server:")) {
    spec.serverId = dest.substr(7);
  } else {
    LOG_TOPIC("77a84", ERR, Logger::COMMUNICATION)
        << "did not understand destination '" << dest << "'";
    co_return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  spec.endpoint = ci.getServerEndpoint(spec.serverId);
  if (spec.endpoint.empty()) {
    if (spec.serverId.find(',') != std::string::npos) {
      TRI_ASSERT(false);
    }
    LOG_TOPIC("f29ef", ERR, Logger::COMMUNICATION)
        << "did not find endpoint of server '" << spec.serverId << "'";
    co_return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }
  co_return TRI_ERROR_NO_ERROR;
}

/// @brief extract the error code form the body
ErrorCode errorCodeFromBody(arangodb::velocypack::Slice body,
                            ErrorCode defaultErrorCode) {
  if (body.isObject()) {
    VPackSlice num = body.get(StaticStrings::ErrorNum);
    if (num.isNumber()) {
      // we found an error number, so let's use it!
      return ErrorCode{num.getNumericValue<int>()};
    }
  }
  return defaultErrorCode;
}

Result resultFromBody(
    std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> const& body,
    ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (body && !body->empty()) {
    return resultFromBody(VPackSlice(body->data()), defaultError);
  }
  return Result(defaultError);
}

Result resultFromBody(
    std::shared_ptr<arangodb::velocypack::Builder> const& body,
    ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (body) {
    return resultFromBody(body->slice(), defaultError);
  }

  return Result(defaultError);
}

Result resultFromBody(arangodb::velocypack::Slice slice,
                      ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (slice.isObject()) {
    if (VPackSlice num = slice.get(StaticStrings::ErrorNum); num.isNumber()) {
      auto errorCode = ErrorCode{num.getNumericValue<int>()};
      if (VPackSlice msg = slice.get(StaticStrings::ErrorMessage);
          msg.isString()) {
        // found an error number and an error message, so let's use it!
        return Result(errorCode, msg.copyString());
      }
      // we found an error number, so let's use it!
      return Result(errorCode);
    }
  }
  return Result(defaultError);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract all error baby-style error codes and store them in a map
////////////////////////////////////////////////////////////////////////////////

void errorCodesFromHeaders(network::Headers headers,
                           std::unordered_map<ErrorCode, size_t>& errorCounter,
                           bool includeNotFound) {
  auto const& codes = headers.find(StaticStrings::ErrorCodes);
  if (codes != headers.end()) {
    auto parsedCodes = VPackParser::fromJson(codes->second);
    VPackSlice codesSlice = parsedCodes->slice();
    if (!codesSlice.isObject()) {
      return;
    }

    for (auto code : VPackObjectIterator(codesSlice)) {
      VPackValueLength codeLength;
      char const* codeString = code.key.getString(codeLength);
      auto codeNr = ErrorCode{
          NumberUtils::atoi_zero<int>(codeString, codeString + codeLength)};
      if (includeNotFound || codeNr != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        errorCounter[codeNr] += code.value.getNumericValue<size_t>();
      }
    }
  }
}

namespace {

ErrorCode toArangoErrorCodeInternal(fuerte::Error err) {
  // This function creates an error code from a fuerte::Error,
  // but only if it is a communication error. If the communication
  // was successful and there was an HTTP error code, this function
  // returns TRI_ERROR_NO_ERROR.
  // If TRI_ERROR_NO_ERROR is returned, then the result was CL_COMM_RECEIVED
  // and .answer can safely be inspected.

  switch (err) {
    case fuerte::Error::NoError:
      return TRI_ERROR_NO_ERROR;

    case fuerte::Error::CouldNotConnect:
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;

    case fuerte::Error::ConnectionClosed:
    case fuerte::Error::CloseRequested:
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;

    case fuerte::Error::RequestTimeout:  // No reply, we give up:
      return TRI_ERROR_CLUSTER_TIMEOUT;

    case fuerte::Error::ConnectionCanceled:
    case fuerte::Error::QueueCapacityExceeded:  // there is no result
    case fuerte::Error::ReadError:
    case fuerte::Error::WriteError:
    case fuerte::Error::ProtocolError:
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
  }

  return TRI_ERROR_INTERNAL;
}
}  // namespace

fuerte::RestVerb arangoRestVerbToFuerte(rest::RequestType verb) {
  switch (verb) {
    case rest::RequestType::DELETE_REQ:
      return fuerte::RestVerb::Delete;
    case rest::RequestType::GET:
      return fuerte::RestVerb::Get;
    case rest::RequestType::POST:
      return fuerte::RestVerb::Post;
    case rest::RequestType::PUT:
      return fuerte::RestVerb::Put;
    case rest::RequestType::HEAD:
      return fuerte::RestVerb::Head;
    case rest::RequestType::PATCH:
      return fuerte::RestVerb::Patch;
    case rest::RequestType::OPTIONS:
      return fuerte::RestVerb::Options;
    case rest::RequestType::ILLEGAL:
      return fuerte::RestVerb::Illegal;
  }

  return fuerte::RestVerb::Illegal;
}

rest::RequestType fuerteRestVerbToArango(fuerte::RestVerb verb) {
  switch (verb) {
    case fuerte::RestVerb::Illegal:
      return rest::RequestType::ILLEGAL;
    case fuerte::RestVerb::Delete:
      return rest::RequestType::DELETE_REQ;
    case fuerte::RestVerb::Get:
      return rest::RequestType::GET;
    case fuerte::RestVerb::Post:
      return rest::RequestType::POST;
    case fuerte::RestVerb::Put:
      return rest::RequestType::PUT;
    case fuerte::RestVerb::Head:
      return rest::RequestType::HEAD;
    case fuerte::RestVerb::Patch:
      return rest::RequestType::PATCH;
    case fuerte::RestVerb::Options:
      return rest::RequestType::OPTIONS;
  }

  return rest::RequestType::ILLEGAL;
}

ErrorCode fuerteToArangoErrorCode(network::Response const& res) {
  LOG_TOPIC_IF("abcde", ERR, Logger::COMMUNICATION,
               res.error != fuerte::Error::NoError)
      << "communication error: '" << fuerte::to_string(res.error)
      << "' from destination '" << res.destination << "'" <<
      [](network::Response const& res) {
        if (res.hasRequest()) {
          return std::string(", url: ") +
                 to_string(res.request().header.restVerb) + " " +
                 res.request().header.path;
        }
        return std::string();
      }(res)
      << ", request ptr: "
      << (res.hasRequest() ? (void*)(&res.request()) : nullptr);
  return toArangoErrorCodeInternal(res.error);
}

ErrorCode fuerteToArangoErrorCode(fuerte::Error err) {
  LOG_TOPIC_IF("abcdf", ERR, Logger::COMMUNICATION,
               err != fuerte::Error::NoError)
      << "communication error: '" << fuerte::to_string(err) << "'";
  return toArangoErrorCodeInternal(err);
}

std::string fuerteToArangoErrorMessage(network::Response const& res) {
  if (res.payloadSize() > 0) {
    // check "errorMessage" attribute first
    velocypack::Slice s = res.slice();
    if (s.isObject()) {
      s = s.get(StaticStrings::ErrorMessage);
      if (s.isString() && s.getStringLength() > 0) {
        return s.copyString();
      }
    }
  }
  return std::string{TRI_errno_string(fuerteToArangoErrorCode(res))};
}

std::string fuerteToArangoErrorMessage(fuerte::Error err) {
  return std::string{TRI_errno_string(fuerteToArangoErrorCode(err))};
}

ErrorCode fuerteStatusToArangoErrorCode(fuerte::Response const& res) {
  return fuerteStatusToArangoErrorCode(res.statusCode());
}

ErrorCode fuerteStatusToArangoErrorCode(fuerte::StatusCode const& statusCode) {
  if (fuerte::statusIsSuccess(statusCode)) {
    return TRI_ERROR_NO_ERROR;
  } else if (statusCode > 0) {
    return ErrorCode{static_cast<int>(statusCode)};
  } else {
    return TRI_ERROR_INTERNAL;
  }
}

std::string fuerteStatusToArangoErrorMessage(fuerte::Response const& res) {
  return fuerteStatusToArangoErrorMessage(res.statusCode());
}

std::string fuerteStatusToArangoErrorMessage(
    fuerte::StatusCode const& statusCode) {
  return fuerte::status_code_to_string(statusCode);
}

void addSourceHeader(consensus::Agent* agent, fuerte::Request& req) {
  // note: agent can be a nullptr here
  auto state = ServerState::instance();
  if (state->isCoordinator() || state->isDBServer()) {
    req.header.addMeta(StaticStrings::ClusterCommSource, state->getId());
#if 0
  } else if (state->isAgent() && agent != nullptr) {
    // note: header intentionally not sent to save cluster-internal
    // traffic
    req.header.addMeta(StaticStrings::ClusterCommSource, agent->id());
#endif
  }
}

void addUserParameter(RequestOptions& reqOpts, std::string_view value) {
  if (!value.empty()) {
    // if no user name is set, we cannot add it to the request options
    // as a URL parameter, because they will assert that the provided
    // value is non-empty
    reqOpts.param(StaticStrings::UserString, value);
  }
}

}  // namespace arangodb::network
