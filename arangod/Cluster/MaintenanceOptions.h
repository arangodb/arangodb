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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/NumberOfCores.h"

#include <algorithm>
#include <cstdint>

namespace arangodb {

struct MaintenanceOptions {
  MaintenanceOptions() {
    // use a quarter of the available cores for maintenance threads by
    // default, but never less than 3 (see MaintenanceFeature::minThreadLimit)
    maintenanceThreadsMax =
        (std::max)(static_cast<uint32_t>(3),
                   static_cast<uint32_t>(NumberOfCores::getValue() / 4 + 1));
    maintenanceThreadsSlowMax = maintenanceThreadsMax / 2;
  }

  /// @brief option for forcing this feature to always be enable - used by the
  /// catch tests
  bool forceActivation = false;

  bool resignLeadershipOnShutdown = false;

  /// @brief tunable option for thread pool size
  uint32_t maintenanceThreadsMax;  // computed in constructor

  /// @brief tunable option for number of slow threads
  uint32_t maintenanceThreadsSlowMax;  // computed in constructor

  /// @brief tunable option for number of seconds COMPLETE or FAILED actions
  /// block duplicates from adding to _actionRegistry
  int32_t secondsActionsBlock = 2;

  /// @brief tunable option for number of seconds COMPLETE and FAILED actions
  /// remain within _actionRegistry
  int32_t secondsActionsLinger = 3600;

  uint64_t maximalNumberOfSyncShardActionsQueued = 32;
};

}  // namespace arangodb
