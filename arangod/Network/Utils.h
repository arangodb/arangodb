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

#pragma once
#ifndef ARANGOD_NETWORK_UTILS_H
#define ARANGOD_NETWORK_UTILS_H 1

#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Network/types.h"
#include "Rest/CommonDefines.h"
#include "Utils/OperationResult.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace velocypack {
class Builder;
}
namespace consensus {
class Agent;
}
class ClusterInfo;
class NetworkFeature;

namespace network {

/// @brief resolve 'shard:' or 'server:' url to actual endpoint
ErrorCode resolveDestination(NetworkFeature const&, DestinationId const& dest,
                             network::EndpointSpec&);
ErrorCode resolveDestination(ClusterInfo&, DestinationId const& dest, network::EndpointSpec&);

Result resultFromBody(std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> const& b,
                      ErrorCode defaultError);
Result resultFromBody(std::shared_ptr<arangodb::velocypack::Builder> const& b,
                      ErrorCode defaultError);
Result resultFromBody(arangodb::velocypack::Slice b, ErrorCode defaultError);

/// @brief extract the error from a cluster response
template <typename T>
OperationResult opResultFromBody(T const& body, ErrorCode defaultErrorCode,
                                 OperationOptions&& options) {
  return OperationResult(arangodb::network::resultFromBody(body, defaultErrorCode),
                         std::move(options));
}

/// @brief extract the error code form the body
ErrorCode errorCodeFromBody(arangodb::velocypack::Slice body,
                            ErrorCode defaultErrorCode = TRI_ERROR_NO_ERROR);

/// @brief Extract all error baby-style error codes and store them in a map
void errorCodesFromHeaders(network::Headers headers,
                           std::unordered_map<ErrorCode, size_t>& errorCounter,
                           bool includeNotFound);

/// @brief transform response into arango error code
ErrorCode fuerteToArangoErrorCode(network::Response const& res);
ErrorCode fuerteToArangoErrorCode(fuerte::Error err);
std::string fuerteToArangoErrorMessage(network::Response const& res);
std::string fuerteToArangoErrorMessage(fuerte::Error err);
ErrorCode fuerteStatusToArangoErrorCode(fuerte::Response const& res);
ErrorCode fuerteStatusToArangoErrorCode(fuerte::StatusCode const& code);
std::string fuerteStatusToArangoErrorMessage(fuerte::Response const& res);
std::string fuerteStatusToArangoErrorMessage(fuerte::StatusCode const& code);

/// @brief convert between arango and fuerte rest methods
fuerte::RestVerb arangoRestVerbToFuerte(rest::RequestType);
rest::RequestType fuerteRestVerbToArango(fuerte::RestVerb);

void addSourceHeader(consensus::Agent* agent, fuerte::Request& req);

}  // namespace network
}  // namespace arangodb

#endif
