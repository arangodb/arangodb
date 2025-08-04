////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Johann Listunov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>
#include <utility>

#include "GeneralServer/RequestLane.h"
#include "Scheduler/Scheduler.h"

#include "../SchedulerMetrics.h"

namespace arangodb {

class AcceptanceQueue {
 public:
  explicit AcceptanceQueue(Scheduler* scheduler,
                           const std::shared_ptr<SchedulerMetrics>& metrics);

  template<typename F,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  [[nodiscard]] bool tryBoundedQueue(RequestLane lane, F&& fn) noexcept {
    if (_scheduler == nullptr) {
      return false;
    }
    return _scheduler->tryBoundedQueue(lane, fn);
  }

  void setLastLowPriorityDequeueTime(uint64_t time);
  [[nodiscard]] Scheduler::QueueStatistics queueStatistics() const;

  bool start();
  void shutdown();

  void trackBeginOngoingLowPriorityTask() noexcept;
  void trackEndOngoingLowPriorityTask() noexcept;

 private:
  Scheduler* _scheduler;
  std::shared_ptr<SchedulerMetrics> _metrics;
};

}  // namespace arangodb
