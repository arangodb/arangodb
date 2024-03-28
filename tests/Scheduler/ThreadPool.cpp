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
#include "Scheduler/SimpleThreadPool.h"

using namespace arangodb;

TEST(ThreadPoolTest, start_stop_test) { ThreadPool pool{1}; }

TEST(ThreadPoolTest, simple_counter) {
  std::atomic<std::size_t> counter{0};
  {
    ThreadPool pool{1};
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
    pool.push([&]() noexcept { counter++; });
  }

  ASSERT_EQ(counter, 3);
}

TEST(ThreadPoolTest, multi_thread_counter) {
  std::atomic<std::size_t> counter{0};
  {
    ThreadPool pool{3};
    for (size_t k = 0; k < 100; k++) {
      pool.push([&]() noexcept { counter++; });
    }
  }

  ASSERT_EQ(counter, 100);
}

TEST(ThreadPoolTest, stop_when_sleeping) {
  {
    ThreadPool pool{3};
    std::this_thread::sleep_for(std::chrono::seconds{3});
  }
}
