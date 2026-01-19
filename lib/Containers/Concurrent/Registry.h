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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/Concurrent/ListOfNonOwnedLists.h"
#include "Containers/Concurrent/metrics.h"
#include "Containers/Concurrent/ThreadOwnedList.h"

namespace arangodb::containers {

template<typename T>
using ThreadRegistry = containers::ThreadOwnedList<T>;

template<typename T>
struct Registry : public containers::ListOfNonOwnedLists<ThreadRegistry<T>> {
  /**
    Metrics-feature is only available after startup, therefore we need to update
    the metrics after construction via this function; will set metrics only
    once.
  */
  auto set_metrics(std::shared_ptr<containers::Metrics> new_metrics) -> void {
    auto guard = std::lock_guard(
        containers::ListOfNonOwnedLists<ThreadRegistry<T>>::_mutex);
    TRI_ASSERT(not metricsAreSet.load(std::memory_order_relaxed));
    metrics = new_metrics;
    metricsAreSet.store(true, std::memory_order_release);
  }
  /**
    Gets metrics, spins if they are not yet set. Spinning is fine because
    metrics are set at startup
   */
  auto get_metrics() -> std::shared_ptr<containers::Metrics> {
    while (true) {
      if (metricsAreSet.load(std::memory_order_acquire)) {
        return metrics;
      }
    }
  }

 private:
  // all thread registries that are added to this registry will use these
  // metrics
  std::shared_ptr<containers::Metrics> metrics;
  std::atomic<bool> metricsAreSet = false;
};

}  // namespace arangodb::containers
