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
#include <string>

namespace arangodb {

struct SchedulerFeatureOptions {
  /// @brief return the default number of threads to use (upper bound)
  static uint64_t getDefaultMaxThreads() noexcept {
    // use two times the number of hardware threads as the default
    // but only if higher than 64. otherwise use a default minimum value of 32
    return (std::max)(static_cast<uint64_t>(32),
                      static_cast<uint64_t>(NumberOfCores::getValue()) * 2);
  }

  SchedulerFeatureOptions() : nrMaximalThreads(getDefaultMaxThreads()) {}

  uint64_t nrMinimalThreads = 4;
  uint64_t nrMaximalThreads;  // computed in constructor
  uint64_t queueSize = 4096;
  uint64_t fifo1Size = 4096;
  uint64_t fifo2Size = 4096;
  uint64_t fifo3Size = 4096;
  double ongoingLowPriorityMultiplier = 4.0;
  double unavailabilityQueueFillGrade = 0.75;
  std::string schedulerType = "supervised";
};

}  // namespace arangodb
