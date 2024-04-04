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
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <deque>

#include "Scheduler.h"

namespace arangodb {

struct ThreadPool {
  using WorkItem = Scheduler::WorkItemBase;

  ThreadPool(const char* name, std::size_t threadCount);
  ~ThreadPool();
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
    std::atomic<uint64_t> done;
    std::atomic<uint64_t> queued;
  };

  Statistics statistics;

 private:
  std::unique_ptr<WorkItem> pop(std::stop_token) noexcept;

  using Clock = std::chrono::steady_clock;

  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::deque<std::unique_ptr<WorkItem>> _tasks;
  std::stop_source _stop;
  std::vector<std::jthread> _threads;
};

}  // namespace arangodb
