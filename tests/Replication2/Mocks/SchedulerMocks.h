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

#include "Replication2/Mocks/IHasScheduler.h"

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

struct FakeScheduler : IScheduler {
  auto delayedFuture(std::chrono::steady_clock::duration delay,
                     [[maybe_unused]] std::string_view name)
      -> futures::Future<futures::Unit> override {
    auto p = futures::Promise<futures::Unit>{};
    auto f = p.getFuture();

    auto thread = std::thread(
        [delay, p = std::move(p), name = std::string(name)]() mutable noexcept {
          // // for debugging
          // __attribute__((used)) thread_local std::string const _name =
          //    std::move(name);
          std::this_thread::sleep_for(delay);
          p.setValue();
        });
    thread.detach();

    return f;
  }

  auto queueDelayed(std::string_view name,
                    std::chrono::steady_clock::duration delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    auto thread =
        std::thread([delay, handler = std::move(handler)]() mutable noexcept {
          std::this_thread::sleep_for(delay);
          std::move(handler)(false);
        });
    thread.detach();
    return nullptr;
  }

  void queue(fu2::unique_function<void()> cb) noexcept override {
    auto thread = std::thread(
        [cb = std::move(cb)]() mutable noexcept { std::move(cb)(); });
    thread.detach();
  }
};

struct DelayedScheduler : IScheduler, IHasScheduler {
  auto delayedFuture(std::chrono::nanoseconds duration, std::string_view name)
      -> arangodb::futures::Future<arangodb::futures::Unit> override {
    if (name.data() == nullptr) {
      name = "replication-2-test";
    }
    auto p = futures::Promise<futures::Unit>{};
    auto f = p.getFuture();
    queueDelayed(name, duration, [p = std::move(p)](bool cancelled) mutable {
      TRI_ASSERT(!cancelled);
      p.setValue();
    });

    return f;
  }

  auto queueDelayed(std::string_view name, std::chrono::nanoseconds delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    // just queue immediately, ignore the delay
    queue([handler = std::move(handler)]() mutable noexcept {
      std::move(handler)(false);
    });
    return nullptr;
  }
  void queue(fu2::unique_function<void()> function) noexcept override {
    _queue.push_back(std::move(function));
  }

  void runOnce() noexcept {
    auto f = std::invoke([this] {
      ADB_PROD_ASSERT(not _queue.empty());
      auto f = std::move(_queue.front());
      _queue.pop_front();
      return f;
    });
    f.operator()();
  }

  auto runAllCurrent() noexcept -> std::size_t {
    auto queue = std::move(_queue);
    auto const tasks = queue.size();
    while (not queue.empty()) {
      runOnce();
    }
    return tasks;
  }

  auto runAll() noexcept -> std::size_t override {
    auto count = std::size_t{0};
    while (hasWork()) {
      runOnce();
      ++count;
    }
    return count;
  }

  auto hasWork() const noexcept -> bool override { return not _queue.empty(); }

  ~DelayedScheduler() override {
    EXPECT_TRUE(_queue.empty())
        << "Unresolved item(s) in the DelayedScheduler queue";
  }

 private:
  std::deque<fu2::unique_function<void()>> _queue;
};

}  // namespace arangodb::replication2::test
