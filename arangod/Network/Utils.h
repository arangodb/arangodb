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
#include "Utils/OperationResult.h"
#include "Utils/OperationOptions.h"

#include <chrono>
#include <fuerte/types.h>
#include <velocypack/Slice.h>
#include <velocypack/Buffer.h>

namespace arangodb {
namespace network {
  
  /// @brief resolve 'shard:' or 'server:' url to actual endpoint
  Result resolveDestination(DestinationId const& dest, std::string&);
  
  /// @brief extract the error from a cluster response
  OperationResult errorCodeFromBody(arangodb::velocypack::Buffer<uint8_t> const& body,
                                    int defaultErrorCode);
  /// @brief extract the error from a cluster response
  OperationResult errorCodeFromBody(std::shared_ptr<VPackBuilder> const&,
                                    int defaultErrorCode);
  /// @brief extract the error from a cluster response
  OperationResult errorCodeFromBody(arangodb::velocypack::Slice const& body,
                                    int defaultErrorCode);
  
  /// @brief Extract all error baby-style error codes and store them in a map
  void errorCodesFromHeaders(network::Headers headers,
                             std::unordered_map<int, size_t>& errorCounter,
                             bool includeNotFound);
  
  /// @brief transform response into arango error code
  int arangoErrorCode(network::Response const& res);
  
  /// @brief Create Cluster Communication result for insert
  OperationResult clusterResultInsert(fuerte::StatusCode responsecode,
                                      std::shared_ptr<velocypack::Buffer<uint8_t>> body,
                                      OperationOptions const& options,
                                      std::unordered_map<int, size_t> const& errorCounter);
  
}}

#endif
