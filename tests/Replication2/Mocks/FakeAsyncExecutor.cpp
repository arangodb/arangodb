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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "FakeAsyncExecutor.h"

#include "Basics/ResultT.h"
#include "Replication2/ReplicatedLog/LogEntry.h"

#include <gtest/gtest.h>

using namespace arangodb::replication2::storage::rocksdb::test;

void ThreadAsyncExecutor::run() noexcept {
  while (true) {
    std::vector<Func> jobs;
    {
      std::unique_lock guard(mutex);
      if (stopping) {
        break;
      } else if (queue.empty()) {
        cv.wait(guard);
      } else {
        jobs = std::move(queue);
        queue.clear();
      }
    }

    for (auto& fn : jobs) {
      fn();
    }
  }
}

ThreadAsyncExecutor::ThreadAsyncExecutor()
    : thread([this]() { this->run(); }) {}

ThreadAsyncExecutor::~ThreadAsyncExecutor() {
  {
    std::unique_lock guard(mutex);
    stopping = true;
  }
  cv.notify_all();
  thread.join();
}

void ThreadAsyncExecutor::operator()(ThreadAsyncExecutor::Func fn) {
  std::unique_lock guard(mutex);
  queue.emplace_back(std::move(fn));
  cv.notify_one();
}

void SyncExecutor::operator()(
    fu2::unique_function<void() noexcept> f) noexcept {
  std::move(f).operator()();
}

DelayedExecutor::~DelayedExecutor() {
  EXPECT_TRUE(queue.empty())
      << "Unresolved item(s) in the DelayedExecutor queue";
}

DelayedExecutor::DelayedExecutor() = default;

void DelayedExecutor::operator()(DelayedExecutor::Func fn) {
  queue.emplace_back(std::move(fn));
}

void DelayedExecutor::runOnce() noexcept { runOnceFromQueue(queue); }

auto DelayedExecutor::hasWork() const noexcept -> bool {
  return not queue.empty();
}

auto DelayedExecutor::runAllCurrent() noexcept -> std::size_t {
  decltype(queue) queue_;
  std::swap(queue_, this->queue);
  auto const tasks = queue_.size();
  while (not queue_.empty()) {
    runOnceFromQueue(queue_);
  }
  return tasks;
}

auto DelayedExecutor::runAll() noexcept -> std::size_t {
  auto count = std::size_t{0};
  while (hasWork()) {
    runOnce();
    ++count;
  }
  return count;
}

void DelayedExecutor::runOnceFromQueue(decltype(queue)& queue_) noexcept {
  auto f = std::invoke([&queue_] {
    ADB_PROD_ASSERT(not queue_.empty());
    auto f = std::move(queue_.front());
    queue_.pop_front();
    return f;
  });
  f.operator()();
}
