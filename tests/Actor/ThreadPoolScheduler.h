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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <functional>
#include "Actor/IScheduler.h"
#include "Basics/ThreadGuard.h"

struct ThreadPoolScheduler : arangodb::actor::IScheduler {
 private:
  arangodb::ThreadGuard threads;
  std::queue<arangodb::actor::LazyWorker> jobs;
  mutable std::mutex queue_mutex;
  std::condition_variable mutex_condition;
  bool should_terminate = false;
  std::uint32_t workingThreads{0};

  auto loop() -> void {
    std::unique_lock lock(queue_mutex);
    while (true) {
      while (not jobs.empty()) {
        auto job = std::move(jobs.front());
        jobs.pop();
        ++workingThreads;
        lock.unlock();
        job();
        lock.lock();
        --workingThreads;
      }
      if (should_terminate) {
        return;
      }
      mutex_condition.wait(lock);
    }
  }

 public:
  auto start(size_t number_of_threads) -> void {
    for (size_t i = 0; i < number_of_threads; i++) {
      threads.emplace([this]() { loop(); });
    }
  }

  auto stop() -> void {
    {
      std::lock_guard lock{queue_mutex};
      should_terminate = true;
    }
    mutex_condition.notify_all();

    threads.joinAll();
  }

  template<class Fn>
  auto isIdle(Fn idleCheck) const -> bool {
    std::lock_guard lock{queue_mutex};
    return jobs.empty() && workingThreads == 0 && idleCheck();
  }

  void queue(arangodb::actor::LazyWorker&& job) override {
    {
      std::unique_lock lock(queue_mutex);
      jobs.push(std::move(job));
    }
    mutex_condition.notify_one();
  }

  void delay(std::chrono::seconds delay,
             std::function<void(bool)>&& job) override {
    // not implemented
  }
};
