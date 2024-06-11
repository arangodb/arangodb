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

#include <atomic>
#include <chrono>
#include <thread>
#include "Scheduler/LockfreeThreadPool.h"
#include "Scheduler/SimpleThreadPool.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Scheduler/WorkStealingThreadPool.h"

using namespace arangodb;

TEST(ThreadPoolTest, start_stop_test) {
  SimpleThreadPool pool{"test-sched", 1};
  // this test basically just checks that we can start and stop the pool and
  // that the destructor does not hang
}

TEST(ThreadPoolTest, simple_counter) {
  std::atomic<std::size_t> counter{0};
  {
    SimpleThreadPool pool{"test-sched", 1};
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
  }

  ASSERT_EQ(counter, 3);
}

TEST(ThreadPoolTest, multi_thread_counter) {
  std::atomic<std::size_t> counter{0};
  {
    SimpleThreadPool pool{"test-sched", 3};
    for (size_t k = 0; k < 100; k++) {
      pool.push([&]() noexcept { counter++; });
    }
  }

  ASSERT_EQ(counter, 100);
}

TEST(ThreadPoolTest, stop_when_sleeping) {
  {
    SimpleThreadPool pool{"test-sched", 3};
    std::this_thread::sleep_for(std::chrono::seconds{3});
  }
  // this test basically just checks that we wake up the sleeping threads to
  // terminate the pool and that the destructor does not hang
}

TEST(ThreadPoolTest, work_when_sleeping) {
  std::atomic<std::size_t> counter{0};
  {
    SimpleThreadPool pool{"test-sched", 3};
    std::this_thread::sleep_for(std::chrono::seconds{3});
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
  }
  ASSERT_EQ(counter, 3);
}
