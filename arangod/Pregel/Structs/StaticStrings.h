////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <Inspection/VPack.h>

#include <Cluster/ClusterTypes.h>

namespace arangodb::pregel::static_strings {

constexpr auto timeStamp = std::string_view{"timeStamp"};
constexpr auto numberVerticesLoaded = std::string_view{"numberVerticesLoaded"};
constexpr auto vertexStorageBytes = std::string_view{"vertexStorageBytes"};
constexpr auto vertexStorageBytesUsed =
    std::string_view{"vertexStorageBytesUsed"};
constexpr auto vertexKeyStorageBytes =
    std::string_view{"vertexKeyStorageBytes"};
constexpr auto vertexKeyStorageBytesUsed =
    std::string_view{"vertexKeyStorageBytesUsed"};
constexpr auto numberEdgesLoaded = std::string_view{"numberEdgesLoaded"};
constexpr auto edgeStorageBytes = std::string_view{"edgeStorageBytes"};
constexpr auto edgeStorageBytesUsed = std::string_view{"edgeStorageBytesUsed"};
constexpr auto edgeKeyStorageBytes = std::string_view{"edgeKeyStorageBytes"};
constexpr auto edgeKeyStorageBytesUsed =
    std::string_view{"edgeKeyStorageBytesUsed"};
constexpr auto workerStatus = std::string_view{"workerStatus"};
constexpr auto timing = std::string_view{"timing"};
constexpr auto loading = std::string_view{"loading"};
constexpr auto running = std::string_view{"running"};
constexpr auto storing = std::string_view{"storing"};
constexpr auto graphStoreStats = std::string_view{"graphStoreStats"};

}  // namespace arangodb::pregel::static_strings
