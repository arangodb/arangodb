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
#pragma once

#include "Replication2/IScheduler.h"
#include <deque>

namespace arangodb::replication2::test {

struct SyncScheduler : IScheduler {
  auto delayedFuture(std::chrono::nanoseconds duration, std::string_view name)
      -> futures::Future<futures::Unit> override {
    std::abort();  // not implemented
  }
  auto queueDelayed(std::string_view name, std::chrono::nanoseconds delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    std::abort();  // not implemented
  }
  void queue(fu2::unique_function<void()> function) noexcept override {
    function();
  }
};

struct AsyncScheduler : IScheduler {
  std::deque<fu2::unique_function<void()>> tasks;

  auto delayedFuture(std::chrono::nanoseconds duration, std::string_view name)
      -> futures::Future<futures::Unit> override {
    std::abort();  // not implemented
  }
  auto queueDelayed(std::string_view name, std::chrono::nanoseconds delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    std::abort();  // not implemented
  }
  void queue(fu2::unique_function<void()> function) noexcept override {
    tasks.emplace_back(std::move(function));
  }

  void runAll() {
    while (!tasks.empty()) {
      std::invoke(tasks.front());
      tasks.pop_front();
    }
  }

  void runFromFront() {
    if (!tasks.empty()) {
      std::invoke(tasks.front());
      tasks.pop_front();
    }
  }

  void runFromBack() {
    if (!tasks.empty()) {
      std::invoke(tasks.back());
      tasks.pop_back();
    }
  }
};

}  // namespace arangodb::replication2::test
