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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Utils.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Basics/NumberUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "VocBase/ticks.h"

#include <fuerte/types.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace network {

ErrorCode resolveDestination(NetworkFeature const& feature, DestinationId const& dest,
                             network::EndpointSpec& spec) {
  // Now look up the actual endpoint:
  if (!feature.server().hasFeature<ClusterFeature>()) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  auto& ci = feature.server().getFeature<ClusterFeature>().clusterInfo();
  return resolveDestination(ci, dest, spec);
}

ErrorCode resolveDestination(ClusterInfo& ci, DestinationId const& dest,
                             network::EndpointSpec& spec) {
  using namespace arangodb;

  if (dest.compare(0, 6, "tcp://", 6) == 0 || dest.compare(0, 6, "ssl://", 6) == 0) {
    spec.endpoint = dest;
    return TRI_ERROR_NO_ERROR;  // all good
  }

  if (dest.compare(0, 11, "http+tcp://", 11) == 0 ||
      dest.compare(0, 11, "http+ssl://", 11) == 0) {
    spec.endpoint = dest.substr(5);
    return TRI_ERROR_NO_ERROR;
  }

  // This sets result.shardId, result.serverId and result.endpoint,
  // depending on what dest is. Note that if a shardID is given, the
  // responsible server is looked up, if a serverID is given, the endpoint
  // is looked up, both can fail and immediately lead to a CL_COMM_ERROR
  // state.

  if (dest.compare(0, 6, "shard:", 6) == 0) {
    spec.shardId = dest.substr(6);
    {
      std::shared_ptr<std::vector<ServerID>> resp = ci.getResponsibleServer(spec.shardId);
      if (!resp->empty()) {
        spec.serverId = (*resp)[0];
      } else {
        LOG_TOPIC("60ee8", ERR, Logger::CLUSTER)
            << "cannot find responsible server for shard '" << spec.shardId << "'";
        return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
      }
    }
    LOG_TOPIC("64670", DEBUG, Logger::CLUSTER) << "Responsible server: " << spec.serverId;
  } else if (dest.compare(0, 7, "server:", 7) == 0) {
    spec.serverId = dest.substr(7);
  } else {
    std::string errorMessage = "did not understand destination '" + dest + "'";
    LOG_TOPIC("77a84", ERR, Logger::COMMUNICATION)
        << "did not understand destination '" << dest << "'";
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  spec.endpoint = ci.getServerEndpoint(spec.serverId);
  if (spec.endpoint.empty()) {
    if (spec.serverId.find(',') != std::string::npos) {
      TRI_ASSERT(false);
    }
    std::string errorMessage =
        "did not find endpoint of server '" + spec.serverId + "'";
    LOG_TOPIC("f29ef", ERR, Logger::COMMUNICATION)
        << "did not find endpoint of server '" << spec.serverId << "'";
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief extract the error code form the body
ErrorCode errorCodeFromBody(arangodb::velocypack::Slice body, ErrorCode defaultErrorCode) {
  if (body.isObject()) {
    VPackSlice num = body.get(StaticStrings::ErrorNum);
    if (num.isNumber()) {
      // we found an error number, so let's use it!
      return ErrorCode{num.getNumericValue<int>()};
    }
  }
  return defaultErrorCode;
}

Result resultFromBody(std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> const& body,
                      ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (body && !body->empty()) {
    return resultFromBody(VPackSlice(body->data()), defaultError);
  }
  return Result(defaultError);
}

Result resultFromBody(std::shared_ptr<arangodb::velocypack::Builder> const& body,
                      ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (body) {
    return resultFromBody(body->slice(), defaultError);
  }

  return Result(defaultError);
}

Result resultFromBody(arangodb::velocypack::Slice slice, ErrorCode defaultError) {
  // read the error number from the response and use it if present
  if (slice.isObject()) {
    VPackSlice num = slice.get(StaticStrings::ErrorNum);
    VPackSlice msg = slice.get(StaticStrings::ErrorMessage);
    if (num.isNumber()) {
      auto errorCode = ErrorCode{num.getNumericValue<int>()};
      if (msg.isString()) {
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
      auto codeNr = ErrorCode{NumberUtils::atoi_zero<int>(codeString, codeString + codeLength)};
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

    case fuerte::Error::VstUnauthorized:
      return TRI_ERROR_FORBIDDEN;
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
  LOG_TOPIC_IF("abcde", ERR, Logger::COMMUNICATION, res.error != fuerte::Error::NoError)
      << "communication error: '" << fuerte::to_string(res.error)
      << "' from destination '" << res.destination << "'";
  return toArangoErrorCodeInternal(res.error);
}

ErrorCode fuerteToArangoErrorCode(fuerte::Error err) {
  LOG_TOPIC_IF("abcdf", ERR, Logger::COMMUNICATION, err != fuerte::Error::NoError)
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

std::string fuerteStatusToArangoErrorMessage(fuerte::StatusCode const& statusCode) {
  return fuerte::status_code_to_string(statusCode);
}

void addSourceHeader(consensus::Agent* agent, fuerte::Request& req) {
  // note: agent can be a nullptr here
  auto state = ServerState::instance();
  if (state->isCoordinator() || state->isDBServer()) {
    req.header.addMeta(StaticStrings::ClusterCommSource, state->getId());
  } else if (state->isAgent() && agent != nullptr) {
    req.header.addMeta(StaticStrings::ClusterCommSource, agent->id());
  }
}

}  // namespace network
}  // namespace arangodb
