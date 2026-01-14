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

#include "Basics/FutureSharedLock.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {

struct AsyncLockWithScheduler {
  struct SchedulerWrapper {
    using WorkHandle = Scheduler::WorkHandle;

    SchedulerWrapper(std::string name) : _name{std::move(name)} {}

    template<typename F>
    void queue(F&& fn) {
      SchedulerFeature::SCHEDULER->queue(RequestLane::CLUSTER_INTERNAL,
                                         std::forward<F>(fn));
    }

    template<typename F>
    WorkHandle queueDelayed(F&& fn, std::chrono::milliseconds timeout) {
      return SchedulerFeature::SCHEDULER->queueDelayed(
          _name, RequestLane::CLUSTER_INTERNAL, timeout, std::forward<F>(fn));
    }

   private:
    std::string _name;
  };

 public:
  using Lock = futures::FutureSharedLock<SchedulerWrapper>::LockGuard;
  AsyncLockWithScheduler(std::string name)
      : _schedulerWrapper{std::move(name)}, _asyncMutex{_schedulerWrapper} {}
  auto lock() -> futures::Future<Lock> {
    return _asyncMutex.asyncLockExclusive();
  }

 private:
  SchedulerWrapper _schedulerWrapper;
  futures::FutureSharedLock<SchedulerWrapper> _asyncMutex;
};
}  // namespace arangodb
