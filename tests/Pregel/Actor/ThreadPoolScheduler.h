////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <functional>

struct ThreadPoolScheduler : std::enable_shared_from_this<ThreadPoolScheduler> {
 private:
  std::vector<std::thread> threads;
  std::queue<std::function<void()>> jobs;
  std::mutex queue_mutex;
  std::condition_variable mutex_condition;
  bool should_terminate = false;

  auto loop() -> void {
    while (true) {
      std::function<void()> job;
      {
        std::unique_lock lock(queue_mutex);
        mutex_condition.wait(lock, [self = this->shared_from_this()] {
          return !self->jobs.empty() || self->should_terminate;
        });
        // all jobs need to be completed before thread ends loop execution
        if (should_terminate && jobs.empty()) {
          return;
        }
        job = std::move(jobs.front());
        jobs.pop();
      }
      job();
    }
  }

 public:
  auto start(size_t number_of_threads) -> void {
    threads.resize(number_of_threads);
    for (size_t i = 0; i < number_of_threads; i++) {
      threads[i] =
          std::thread([self = this->shared_from_this()]() { self->loop(); });
    }
  }

  auto stop() -> void {
    {
      std::unique_lock lock(queue_mutex);
      should_terminate = true;
    }
    mutex_condition.notify_all();
    for (auto& active_thread : threads) {
      active_thread.join();
    }
    threads.clear();
  }

  auto operator()(std::function<void()> job) -> void {
    {
      std::unique_lock lock(queue_mutex);
      jobs.push(job);
    }
    mutex_condition.notify_one();
  }
};
