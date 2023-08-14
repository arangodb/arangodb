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

#include "Replication2/Storage/RocksDB/AsyncLogWriteBatcher.h"

#include <deque>
#include <thread>

namespace arangodb::replication2::storage::rocksdb::test {

struct ThreadAsyncExecutor : AsyncLogWriteBatcher::IAsyncExecutor {
  using Func = fu2::unique_function<void() noexcept>;

  void operator()(Func fn) override;

  ~ThreadAsyncExecutor() override;
  ThreadAsyncExecutor();

 private:
  void run() noexcept;

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<Func> queue;
  bool stopping{false};
  // initialize thread last, so the thread cannot access uninitialized members
  std::thread thread;
};

struct DelayedExecutor : AsyncLogWriteBatcher::IAsyncExecutor,
                         arangodb::replication2::test::IHasScheduler {
  using Func = fu2::unique_function<void() noexcept>;

  void operator()(Func fn) override;

  ~DelayedExecutor() override;
  DelayedExecutor();

  auto hasWork() const noexcept -> bool override;
  auto runAll() noexcept -> std::size_t override;

  void runOnce() noexcept;
  auto runAllCurrent() noexcept -> std::size_t;

 private:
  std::deque<Func> queue;
};

struct SyncExecutor : AsyncLogWriteBatcher::IAsyncExecutor {
  void operator()(fu2::unique_function<void() noexcept> f) noexcept override;
};

}  // namespace arangodb::replication2::storage::rocksdb::test
