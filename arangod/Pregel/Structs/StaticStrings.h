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
constexpr auto verticesLoaded = std::string_view{"verticesLoaded"};
constexpr auto edgesLoaded = std::string_view{"edgesLoaded"};
constexpr auto workerStatus = std::string_view{"workerStatus"};

}  // namespace arangodb::pregel::static_strings
