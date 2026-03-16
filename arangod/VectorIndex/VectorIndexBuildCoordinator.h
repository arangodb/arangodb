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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include <stop_token>
#include <thread>

namespace arangodb {

class DatabaseFeature;

/// Single background thread that periodically scans for untrained vector
/// indexes and builds them one at a time. The same thread scans and builds.
class VectorIndexBuildCoordinator {
 public:
  static constexpr auto kScanInterval = std::chrono::seconds(5);
  static constexpr auto kSleepGranularity = std::chrono::milliseconds(100);

  explicit VectorIndexBuildCoordinator(DatabaseFeature& dbFeature);

  void start();
  void beginShutdown();
  void stop();

 private:
  void run(std::stop_token stopToken);

  DatabaseFeature& _dbFeature;
  std::jthread _thread;
};

}  // namespace arangodb
