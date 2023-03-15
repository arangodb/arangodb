////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "FakeAsyncExecutor.h"

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

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

void DelayedExecutor::runOnce() noexcept {
  auto f = std::invoke([this] {
    ADB_PROD_ASSERT(not queue.empty());
    auto f = std::move(queue.front());
    queue.pop_front();
    return f;
  });
  f.operator()();
}

void DelayedExecutor::runAll() noexcept {
  while (not queue.empty()) {
    runOnce();
  }
}

auto DelayedExecutor::hasWork() const noexcept -> bool {
  return not queue.empty();
}
