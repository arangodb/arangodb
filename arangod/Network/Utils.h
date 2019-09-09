////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_NETWORK_UTILS_H
#define ARANGOD_NETWORK_UTILS_H 1

#include "Basics/Result.h"
#include "Network/types.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <chrono>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace network {

/// @brief resolve 'shard:' or 'server:' url to actual endpoint
int resolveDestination(DestinationId const& dest, std::string&);

/// @brief extract the error from a cluster response
OperationResult opResultFromBody(arangodb::velocypack::Buffer<uint8_t> const& body,
                                 int defaultErrorCode);
/// @brief extract the error from a cluster response
OperationResult opResultFromBody(std::shared_ptr<arangodb::velocypack::Builder> const&,
                                 int defaultErrorCode);
/// @brief extract the error from a cluster response
OperationResult opResultFromBody(arangodb::velocypack::Slice body, int defaultErrorCode);
  
Result resultFromBody(std::shared_ptr<arangodb::velocypack::Builder> const& body,
                      int defaultError);
Result resultFromBody(arangodb::velocypack::Slice body,
                      int defaultError);

/// @brief extract the error code form the body
int errorCodeFromBody(arangodb::velocypack::Slice body);

/// @brief Extract all error baby-style error codes and store them in a map
void errorCodesFromHeaders(network::Headers headers,
                           std::unordered_map<int, size_t>& errorCounter,
                           bool includeNotFound);

/// @brief transform response into arango error code
int fuerteToArangoErrorCode(network::Response const& res);

/// @brief Create Cluster Communication result for insert
OperationResult clusterResultInsert(fuerte::StatusCode responsecode,
                                    std::shared_ptr<velocypack::Buffer<uint8_t>> body,
                                    OperationOptions const& options,
                                    std::unordered_map<int, size_t> const& errorCounter);
OperationResult clusterResultDocument(arangodb::fuerte::StatusCode code,
                                      std::shared_ptr<VPackBuffer<uint8_t>> body,
                                      OperationOptions const& options,
                                      std::unordered_map<int, size_t> const& errorCounter);

}  // namespace network
}  // namespace arangodb

#endif
