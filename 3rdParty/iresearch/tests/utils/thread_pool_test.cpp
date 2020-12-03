////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "tests_shared.hpp"
#include "utils/async_utils.hpp"
#include "utils/misc.hpp"

namespace {

class notifying_counter {
 public:
  notifying_counter(std::condition_variable& cond, size_t notify_after): cond_(cond), count_(0), notify_after_(notify_after) {}
  notifying_counter& operator++() { std::lock_guard<decltype(lock_)> lock(lock_); if (++count_ >= notify_after_) cond_.notify_all(); return *this; }
  explicit operator bool() { std::lock_guard<decltype(lock_)> lock(lock_); return count_ >= notify_after_; }

 private:
  std::condition_variable& cond_;
  size_t count_;
  std::mutex lock_;
  size_t notify_after_;
};

}

using namespace std::chrono_literals;

TEST(thread_pool_test, test_run_1task_mt) {
  // test schedule 1 task
  irs::async_utils::thread_pool pool(1, 0);
  std::condition_variable cond;
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  auto task = [&mutex, &cond]()->void {
    std::lock_guard<std::mutex> lock(mutex);
    cond.notify_all();
  };

  ASSERT_TRUE(pool.run(std::move(task)));
  ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms));
}

TEST(thread_pool_test, test_run_3tasks_seq_mt) {
  // test schedule 3 task sequential
  irs::async_utils::thread_pool pool(1, 1);
  std::condition_variable cond;
  notifying_counter count(cond, 3);
  std::mutex mutex;
  std::mutex sync_mutex;
  auto task1 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(300ms); };
  auto task2 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(300ms); };
  auto task3 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(300ms); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  ASSERT_TRUE(pool.run(std::move(task3)));
  ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 10000ms)); // wait for all 3 tasks
  pool.stop();
}

TEST(thread_pool_test, test_run_3tasks_parallel_mt) {
  // test schedule 3 task parallel
  irs::async_utils::thread_pool pool(3, 0);
  std::condition_variable cond;
  notifying_counter count(cond, 3);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  auto task3 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  ASSERT_TRUE(pool.run(std::move(task3)));
  ASSERT_TRUE(count || std::cv_status::no_timeout == cond.wait_for(lock, 1000ms) || count); // wait for all 3 tasks
  lock.unlock();
  pool.stop();
}

TEST(thread_pool_test, test_run_1task_excpetion_1task_mt) {
  // test schedule 1 task exception + 1 task
  irs::async_utils::thread_pool pool(1, 0, IR_NATIVE_STRING("foo"));
  std::condition_variable cond;
  notifying_counter count(cond, 2);
  std::mutex mutex;
  auto task1 = [&count]()->void { ++count; throw "error"; };
  auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  std::unique_lock<std::mutex> lock(mutex);
  std::mutex dummy_mutex;
  std::unique_lock<std::mutex> dummy_lock(dummy_mutex);

  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  ASSERT_TRUE(count || std::cv_status::no_timeout == cond.wait_for(dummy_lock, 10000ms) || count); // wait for all 2 tasks (exception trace is slow on MSVC and even slower on *NIX with gdb)
  ASSERT_EQ(1, pool.threads());
  lock.unlock();
  pool.stop(true);
}

TEST(thread_pool_test, test_max_threads_mt) {
  // test max threads
  irs::async_utils::thread_pool pool(0, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  auto task3 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_EQ(0, pool.threads());
  pool.run(std::move(task1));
  pool.run(std::move(task2));
  pool.run(std::move(task3));
  ASSERT_EQ(3, pool.tasks_pending());
  pool.max_threads(2);
  std::this_thread::sleep_for(100ms); // assume threads start within 100msec
  ASSERT_EQ(2, count); // 2 tasks started
  ASSERT_EQ(2, pool.threads());
  ASSERT_EQ(2, pool.tasks_active());
  ASSERT_EQ(1, pool.tasks_pending());
  lock.unlock();
  pool.stop(true);
}

TEST(thread_pool_test, test_max_threads_delta_grow_mt) {
  // test max threads delta grow
  irs::async_utils::thread_pool pool(0, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_EQ(0, pool.threads());
  pool.run(std::move(task));
  pool.max_threads_delta(1);
  std::this_thread::sleep_for(100ms); // assume threads start within 100msec
  ASSERT_EQ(1, count); // 1 task started
  ASSERT_EQ(1, pool.threads());
  ASSERT_EQ(1, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  lock.unlock();
  pool.stop(true);
}

TEST(thread_pool_test, test_max_threads_delta_mt) {
  // test max threads delta
  irs::async_utils::thread_pool pool(1, 10);

  ASSERT_EQ(1, pool.max_threads());
  pool.max_threads_delta(1);
  ASSERT_EQ(2, pool.max_threads());
  pool.max_threads_delta(-2);
  ASSERT_EQ(0, pool.max_threads());
  pool.max_threads(std::numeric_limits<size_t>::max());
  pool.max_threads_delta(1);
  ASSERT_EQ(std::numeric_limits<size_t>::max(), pool.max_threads());
  pool.max_threads(1);
  pool.max_threads_delta(-2);
  ASSERT_EQ(std::numeric_limits<size_t>::min(), pool.max_threads());
}

TEST(thread_pool_test, test_max_idle_mt) {
  // test max idle
  irs::async_utils::thread_pool pool(0, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex1;
  std::mutex mutex2;
  std::condition_variable start_cond;
  notifying_counter start_count(start_cond, 3);
  std::mutex start_mutex;
  auto task1 = [&start_mutex, &start_count, &mutex1, &count]()->void { { std::lock_guard<std::mutex> lock(start_mutex); } ++start_count; std::lock_guard<std::mutex> lock(mutex1); ++count; };
  auto task2 = [&start_mutex, &start_count, &mutex1, &count]()->void { { std::lock_guard<std::mutex> lock(start_mutex); } ++start_count; std::lock_guard<std::mutex> lock(mutex1); ++count; };
  auto task3 = [&start_mutex, &start_count, &mutex2, &count]()->void { { std::lock_guard<std::mutex> lock(start_mutex); } ++start_count; std::lock_guard<std::mutex> lock(mutex2); ++count; };
  std::unique_lock<std::mutex> lock1(mutex1);
  std::unique_lock<std::mutex> lock2(mutex2);
  std::unique_lock<std::mutex> start_lock(start_mutex);

  ASSERT_EQ(0, pool.threads());
  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  ASSERT_TRUE(pool.run(std::move(task3)));
  pool.limits(3, 1);
  ASSERT_EQ(std::make_pair(size_t(3), size_t(1)), pool.limits());
  ASSERT_TRUE(start_count || std::cv_status::no_timeout == start_cond.wait_for(start_lock, 1000ms) || start_count); // wait for all 3 tasks to start
  ASSERT_EQ(0, count); // 0 tasks complete
  ASSERT_EQ(3, pool.threads());
  ASSERT_EQ(3, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(std::make_tuple(size_t(3), size_t(0), size_t(3)), pool.stats());
  lock1.unlock();
  std::this_thread::sleep_for(100ms); // assume threads finish within 100msec
  ASSERT_EQ(2, count); // 2 tasks complete
  ASSERT_EQ(2, pool.threads());
  lock2.unlock();
  pool.stop(true);
}

TEST(thread_pool_test, test_max_idle_delta_mt) {
  // test max idle delta
  irs::async_utils::thread_pool pool(10, 1);

  ASSERT_EQ(1, pool.max_idle());
  pool.max_idle_delta(1);
  ASSERT_EQ(2, pool.max_idle());
  pool.max_idle_delta(-2);
  ASSERT_EQ(0, pool.max_idle());
  pool.max_idle(std::numeric_limits<size_t>::max());
  pool.max_idle_delta(1);
  ASSERT_EQ(std::numeric_limits<size_t>::max(), pool.max_idle());
  pool.max_idle(1);
  pool.max_idle_delta(-2);
  ASSERT_EQ(std::numeric_limits<size_t>::min(), pool.max_idle());
}

TEST(thread_pool_test, test_stop_run_pending_delay_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(1, 1);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void {
    ++count;
    { std::lock_guard<std::mutex> lock(mutex); }
    std::this_thread::sleep_for(300ms);
  };
  auto task2 = [&mutex, &count]()->void {
    ++count;
    { std::lock_guard<std::mutex> lock(mutex); }
    std::this_thread::sleep_for(300ms);
  };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1), 30ms));
  ASSERT_TRUE(pool.run(std::move(task2), 500ms));
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (1 != pool.tasks_pending() && 1 != pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  lock.unlock();
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (count.load() < 2) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(1, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(1)), pool.stats());
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(2, count); // all tasks ran
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_schedule_seq_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(1, 1);
  size_t id(0);
  std::mutex mutex;
  std::condition_variable cond;
  auto task1 = [&mutex, &cond, &id]()->void {
    {
      std::lock_guard<std::mutex> lock(mutex);
      id = 1;
    }
    cond.notify_one();
    std::this_thread::sleep_for(301ms);
  };
  auto task2 = [&mutex, &cond, &id]()->void {
    {
      std::lock_guard<std::mutex> lock(mutex);
      id = 2;
    }
    cond.notify_one();
    std::this_thread::sleep_for(300ms);
  };
  auto task3 = [&mutex, &cond, &id]()->void {
    {
      std::lock_guard<std::mutex> lock(mutex);
      id = 3;
    }
    cond.notify_one();
    std::this_thread::sleep_for(301ms);
  };
  std::unique_lock<std::mutex> lock(mutex);
  ASSERT_TRUE(pool.run(std::move(task1), 1000ms));
  ASSERT_TRUE(pool.run(std::move(task2), 100ms));
  ASSERT_TRUE(pool.run(std::move(task3), 500ms));
  cond.wait_for(lock, 10s);
  ASSERT_EQ(id, 2);
  cond.wait_for(lock, 10s);
  ASSERT_EQ(id, 3);
  cond.wait_for(lock, 10s);
  ASSERT_EQ(id, 1);

  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (1 != pool.tasks_pending() && 1 != pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  lock.unlock();
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(1, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(1)), pool.stats());
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_stop_long_running_run_after_stop_mt) {
  std::function<void()> func = []() { EXPECT_FALSE(true); };

  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<size_t> count{0};
  auto long_running_task = [&mtx, &count]() {
    {
      auto lock = irs::make_lock_guard(mtx);
    }

    std::this_thread::sleep_for(2s);
    ++count;
  };

  irs::async_utils::thread_pool pool(1, 0);
  {
    auto lock = irs::make_lock_guard(mtx);
    ASSERT_TRUE(pool.run(std::move(long_running_task)));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (1 != pool.tasks_active()) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
    ASSERT_EQ(1, pool.tasks_active());
    ASSERT_EQ(0, pool.tasks_pending());
    ASSERT_EQ(1, pool.threads());
    ASSERT_EQ(std::make_tuple(size_t(1),size_t(0),size_t(1)), pool.stats());
  }
  ASSERT_FALSE(pool.run(std::function<void()>()));
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(1, count);
  ASSERT_FALSE(pool.run(std::move(func)));
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_stop_long_ruuning_run_skip_pending_mt) {
  std::mutex mtx;
  std::atomic<size_t> count{0};
  auto long_running_task = [&mtx, &count]() {
    {
      auto lock = irs::make_lock_guard(mtx);
    }

    std::this_thread::sleep_for(2s);

    ++count;
  };

  irs::async_utils::thread_pool pool(1, 0);
  {
    auto lock = irs::make_unique_lock(mtx);
    ASSERT_TRUE(pool.run(std::move(long_running_task)));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (1 != pool.tasks_active()) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
    ASSERT_EQ(1, pool.tasks_active());
    ASSERT_EQ(0, pool.tasks_pending());
    ASSERT_EQ(1, pool.threads());
    ASSERT_EQ(std::make_tuple(size_t(1),size_t(0),size_t(1)), pool.stats());

    for (size_t i = 0; i < 100; ++i) {
      ASSERT_TRUE(pool.run([&count](){++count;}, 10ms));
    }

    ASSERT_EQ(1, pool.tasks_active());
    ASSERT_EQ(100, pool.tasks_pending());
    ASSERT_EQ(1, pool.threads());
    ASSERT_EQ(std::make_tuple(size_t(1),size_t(100),size_t(1)), pool.stats());

    lock.unlock();
    pool.stop(true);
  }

  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.threads()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  ASSERT_EQ(1, count);
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_stop_run_pending_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(4, 2);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  auto task3 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  auto task4 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1), 0ms));
  ASSERT_TRUE(pool.run(std::move(task2), 0ms));
  ASSERT_TRUE(pool.run(std::move(task3), 30ms));
  ASSERT_TRUE(pool.run(std::move(task4), 500ms));
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (count.load() < 4) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  lock.unlock();
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_pending()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(2, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(2)), pool.stats());
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(4, count); // all tasks ran
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_stop_with_pending_task_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(1, 1);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  auto task3 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  {
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(pool.run(std::move(task1), 0ms));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (count.load() < 1) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_pending()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(1, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(1)), pool.stats());
  {
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(pool.run(std::move(task2), 500ms));
    ASSERT_TRUE(pool.run(std::move(task3), 5000ms));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (count.load() < 2) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
  }
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(3, count); // all tasks ran
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_abort_with_pending_task_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(1, 1);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } };
  auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  auto task3 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  {
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(pool.run(std::move(task1), 0ms));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (count.load() < 1) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_active()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  {
    const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
    while (pool.tasks_pending()) {
      std::this_thread::sleep_for(100ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(1, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(1)), pool.stats());
  {
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(pool.run(std::move(task2), 500ms));
    ASSERT_TRUE(pool.run(std::move(task3), 5000ms));
    {
      const auto end = std::chrono::steady_clock::now() + 10s; // assume 10s is more than enough
      while (count.load() < 2) {
        std::this_thread::sleep_for(100ms);
        ASSERT_LE(std::chrono::steady_clock::now(), end);
      }
    }
  }
  pool.stop(true); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(2, count); // task3 didn't run
  ASSERT_EQ(0, pool.tasks_active());
  ASSERT_EQ(0, pool.tasks_pending());
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(std::make_tuple(size_t(0),size_t(0),size_t(0)), pool.stats());
}

TEST(thread_pool_test, test_stop_mt) {
  // test stop run pending
  irs::async_utils::thread_pool pool(1, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  std::this_thread::sleep_for(100ms); // assume threads start within 100msec (2 threads)
  lock.unlock();
  pool.stop(); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(2, count); // all tasks ran
}

TEST(thread_pool_test, test_stop_skip_pending_mt) {
  // test stop skip pending
  irs::async_utils::thread_pool pool(1, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(300ms); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_TRUE(pool.run(std::move(task1)));
  ASSERT_TRUE(pool.run(std::move(task2)));
  std::this_thread::sleep_for(100ms); // assume threads start within 100msec (1 thread)
  lock.unlock();
  pool.stop(true); // blocking call (thread runtime duration simulated via sleep)
  ASSERT_EQ(1, count); // only 1 task ran
}

TEST(thread_pool_test, test_stop_run_mt) {
  // test pool stop + run
  irs::async_utils::thread_pool pool(1, 0);
  std::atomic<size_t> count(0);
  std::mutex mutex;
  auto task1 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
  std::unique_lock<std::mutex> lock(mutex);

  ASSERT_EQ(0, pool.threads());
  ASSERT_TRUE(pool.run(std::move(task1)));
  pool.max_threads(1);
  std::this_thread::sleep_for(100ms); // assume threads start within 100msec
  ASSERT_EQ(1, count); // 1 task started
  ASSERT_EQ(1, pool.threads());
  lock.unlock();
  pool.stop(true);
  ASSERT_FALSE(pool.run(std::move(task2)));
}

TEST(thread_pool_test, test_multiple_calls_stop_mt) {
  // test multiple calls to stop will all block
  irs::async_utils::thread_pool pool(1, 0);
  std::condition_variable cond;
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  auto task = [&mutex, &cond]()->void { std::lock_guard<std::mutex> lock(mutex); cond.notify_all(); };

  ASSERT_EQ(0, pool.threads());
  ASSERT_TRUE(pool.run(std::move(task)));
  ASSERT_EQ(1, pool.threads());

  std::condition_variable cond2;
  std::mutex mutex2;
  std::unique_lock<std::mutex> lock2(mutex2);
  std::atomic<bool> stop(false);
  std::thread thread1([&pool, &mutex2, &cond2, &stop]()->void { pool.stop(); stop = true; std::lock_guard<std::mutex> lock(mutex2); cond2.notify_all(); });
  std::thread thread2([&pool, &mutex2, &cond2, &stop]()->void { pool.stop(); stop = true; std::lock_guard<std::mutex> lock(mutex2); cond2.notify_all(); });

  auto result = cond.wait_for(lock2, 1000ms); // assume thread blocks in 1000ms

  // As declaration for wait_for contains "It may also be unblocked spuriously." for all platforms
  while(!stop && result == std::cv_status::no_timeout) result = cond2.wait_for(lock2, 1000ms);

  ASSERT_EQ(std::cv_status::timeout, result);
  // ^^^ expecting timeout because pool should block indefinitely
  lock2.unlock();
  ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms));
  thread1.join();
  thread2.join();
}

TEST(thread_pool_test, test_stop_signle_threads_mt) {
  // test stop with a single thread will stop threads
  irs::async_utils::thread_pool pool(1, 1);

  ASSERT_TRUE(pool.run([]()->void{})); // start a single thread
  ASSERT_EQ(1, pool.threads());
  pool.stop();
  ASSERT_EQ(0, pool.threads());
}

#if (defined(__linux__) || defined(__APPLE__) || (defined(_WIN32) && (_WIN32_WINNT >= _WIN32_WINNT_WIN10)))
TEST(thread_pool_test, test_check_name_mt) {
  // test stop with a single thread will stop threads
  const thread_name_t expected_name = IR_NATIVE_STRING("foo");
  std::basic_string<std::remove_pointer_t<thread_name_t>> actual_name;
  irs::async_utils::thread_pool pool(1, 1, IR_NATIVE_STRING("foo"));

  ASSERT_TRUE(pool.run([expected_name, &actual_name]()->void{
    EXPECT_TRUE(irs::set_thread_name(expected_name));
    EXPECT_TRUE(irs::get_thread_name(actual_name));
  })); // start a single thread
  ASSERT_EQ(1, pool.threads());
  pool.stop();
  ASSERT_EQ(0, pool.threads());
  ASSERT_EQ(expected_name, actual_name);
}
#endif
