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

#include <gtest/gtest.h>
#include <thread>
#include "Scheduler/SimpleThreadPool.h"

using namespace arangodb;

TEST(ThreadPoolTest, start_stop_test) { ThreadPool pool{"test-sched", 1}; }

TEST(ThreadPoolTest, simple_counter) {
  std::atomic<std::size_t> counter{0};
  {
    ThreadPool pool{"test-sched", 1};
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
  }

  ASSERT_EQ(counter, 3);
}

TEST(ThreadPoolTest, multi_thread_counter) {
  std::atomic<std::size_t> counter{0};
  {
    ThreadPool pool{"test-sched", 3};
    for (size_t k = 0; k < 100; k++) {
      pool.push([&]() noexcept { counter++; });
    }
  }

  ASSERT_EQ(counter, 100);
}

TEST(ThreadPoolTest, stop_when_sleeping) {
  {
    ThreadPool pool{"test-sched", 3};
    std::this_thread::sleep_for(std::chrono::seconds{3});
  }
}

TEST(ThreadPoolTest, work_when_sleeping) {
  std::atomic<std::size_t> counter{0};
  {
    ThreadPool pool{"test-sched", 3};
    std::this_thread::sleep_for(std::chrono::seconds{3});
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
  }
  ASSERT_EQ(counter, 3);
}

namespace {
struct callable;
callable createLambda(std::atomic<std::uint32_t>& cnt, ThreadPool& pool, int x,
                      int max);

struct callable {
  void operator()() const noexcept {
    auto v = cnt.fetch_add(1);
    if (v + 1 == (2ull << max) - 1) {
      ASSERT_EQ(x, max);
      cnt.notify_one();
    }
    if (x < max) {
      pool.push(createLambda(cnt, pool, x + 1, max));
      pool.push(createLambda(cnt, pool, x + 1, max));
    }
  }

  std::atomic<std::uint32_t>& cnt;
  ThreadPool& pool;
  const int x;
  const int max;
};

callable createLambda(std::atomic<std::uint32_t>& cnt, ThreadPool& pool, int x,
                      int max) {
  return callable{cnt, pool, x, max};
}
}  // namespace

TEST(ThreadPoolTest, spawn_work) {
  std::atomic<std::uint32_t> counter{0};
  const auto max = 21;

  {
    ThreadPool pool{"test-sched", 16};
    pool.push(createLambda(counter, pool, 0, max));
    auto v = counter.load();
    const auto expected = (2ull << max) - 1;
    while (v != expected) {
      counter.wait(v);
      v = counter.load();
    }
  }
  ASSERT_EQ(counter, (2ull << max) - 1);
}
