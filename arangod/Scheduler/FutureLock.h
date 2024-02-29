//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/FutureSharedLock.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

// This file specializes the `FutureSharedLock` template class from
// `Basics/FutureSharedLock.h` to use the `arangod` Scheduler singleton
// in SchedulerFeature. To this end, we define and implement a
// `SchedulerWrapper` class, which knows our central Scheduler singleton.

namespace arangodb {

struct SchedulerWrapper {
  using WorkHandle = Scheduler::WorkHandle;
  template<typename F>
  void queue(F&& fn) {
    SchedulerFeature::SCHEDULER->queue(RequestLane::CLUSTER_INTERNAL,
                                       std::forward<F>(fn));
  }
  template<typename F>
  WorkHandle queueDelayed(F&& fn, std::chrono::milliseconds timeout) {
    return SchedulerFeature::SCHEDULER->queueDelayed(
        "rocksdb-meta-collection-lock-timeout", RequestLane::CLUSTER_INTERNAL,
        timeout, std::forward<F>(fn));
  }
};

using FutureLock = arangodb::futures::FutureSharedLock<SchedulerWrapper>;

}  // namespace arangodb
