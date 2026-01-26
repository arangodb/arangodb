////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <string>

namespace arangodb {

struct ApiRecordingFeatureOptions {
  // Whether or not to record recent API calls
  bool enabledCalls = true;

  // Whether or not to record recent AQL queries
  bool enabledQueries = true;

  // Total memory limit for all ApiCallRecord lists combined
  size_t totalMemoryLimitCalls = 25 * (std::size_t{1} << 20);  // Default: 25MiB

  // Total memory limit for all AqlCallRecord lists combined
  size_t totalMemoryLimitQueries =
      25 * (std::size_t{1} << 20);  // Default: 25MiB

  // API permission control
  std::string apiSwitch = "true";
  bool apiEnabled = true;
};

}  // namespace arangodb
