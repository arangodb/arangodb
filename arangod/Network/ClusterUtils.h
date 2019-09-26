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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGOD_NETWORK_CLUSTER_UTILS_H
#define ARANGOD_NETWORK_CLUSTER_UTILS_H 1

#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace network {

/// @brief Create Cluster Communication result for insert
OperationResult clusterResultInsert(fuerte::StatusCode responsecode,
                                    std::shared_ptr<velocypack::Buffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<int, size_t> const& errorCounter);
OperationResult clusterResultDocument(arangodb::fuerte::StatusCode code,
                                      std::shared_ptr<VPackBuffer<uint8_t>> body,
                                      OperationOptions options,
                                      std::unordered_map<int, size_t> const& errorCounter);
OperationResult clusterResultModify(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<int, size_t> const& errorCounter);
OperationResult clusterResultDelete(arangodb::fuerte::StatusCode code,
                                    std::shared_ptr<VPackBuffer<uint8_t>> body,
                                    OperationOptions options,
                                    std::unordered_map<int, size_t> const& errorCounter);

}  // namespace network
}  // namespace arangodb

#endif
