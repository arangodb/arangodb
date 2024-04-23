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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstddef>
#include <latch>
#include <memory>
#include <thread>
#include <vector>

#include "Assertions/Assert.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ThreadPoolMetrics.h"

namespace arangodb {

struct WorkStealingThreadPool {
  struct WorkItem : Scheduler::WorkItemBase {
    ~WorkItem() override { TRI_ASSERT(next == nullptr); }
    WorkItem* next = nullptr;
  };

  WorkStealingThreadPool(const char* name, std::size_t threadCount,
                         ThreadPoolMetrics metrics = {});
  ~WorkStealingThreadPool();

  void shutdown() noexcept;

  void push(std::unique_ptr<WorkItem>&& task) noexcept;

  template<std::invocable F>
  void push(F&& fn) noexcept {
    struct LambdaTask : WorkItem {
      F _func;
      explicit LambdaTask(F&& func) : _func(std::forward<F>(func)) {}

      static_assert(std::is_nothrow_invocable_r_v<void, F>);
      void invoke() noexcept override { std::forward<F>(_func)(); }
    };

    push(std::make_unique<LambdaTask>(std::forward<F>(fn)));
  }

  struct Statistics {
    std::atomic<uint64_t> done{0};
    std::atomic<uint64_t> queued{0};
    std::atomic<uint64_t> dequeued{0};
  };

  Statistics statistics;

  const std::size_t numThreads;

 private:
  struct ThreadState;

  using Clock = std::chrono::steady_clock;

  ThreadPoolMetrics _metrics;

  std::atomic<std::size_t> pushIdx = 0;
  std::atomic<ThreadState*> hint = nullptr;
  std::vector<std::jthread> _threads;
  std::vector<std::unique_ptr<ThreadState>> _threadStates;
  std::latch _latch;
};

}  // namespace arangodb
