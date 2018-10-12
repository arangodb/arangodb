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

#include "gtest/gtest.h"
#include "utils/async_utils.hpp"

namespace tests {
  class async_utils_tests: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };

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

using namespace tests;

TEST_F(async_utils_tests, test_busywait_mutex_mt) {
  typedef iresearch::async_utils::busywait_mutex mutex_t;
  // lock + unlock
  {
    mutex_t mutex;
    std::lock_guard<mutex_t> lock(mutex);
    std::thread thread([&mutex]()->void{ ASSERT_FALSE(mutex.try_lock()); });
    thread.join();
  }

  // recursive lock
  {
    mutex_t mutex;
    std::lock_guard<mutex_t> lock(mutex);
    ASSERT_FALSE(mutex.try_lock());
    #ifdef IRESEARCH_DEBUG
      ASSERT_DEATH(mutex.lock(), "");
    #else
      // for non-debug mutex.lock() will never return
    #endif
  }

  // unlock not owned
  {
    std::condition_variable cond;
    std::mutex ctrl_mtx;
    std::unique_lock<std::mutex> lock(ctrl_mtx);
    mutex_t mutex;
    std::thread thread([&cond, &ctrl_mtx, &mutex]()->void {
      std::unique_lock<std::mutex> lock(ctrl_mtx);
      mutex.lock();
      cond.notify_all();
      cond.wait_for(lock, std::chrono::milliseconds(1000));
      mutex.unlock();
    });

    ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000)));
    #ifdef IRESEARCH_DEBUG
      ASSERT_DEATH(mutex.unlock(), "");
    #else
      // for non-debug this will be a NOOP
    #endif
    lock.unlock();
    cond.notify_all();
    thread.join();
  }
}

TEST_F(async_utils_tests, test_read_write_mutex_mt) {
  typedef iresearch::async_utils::read_write_mutex mutex_t;
  typedef iresearch::async_utils::read_write_mutex::read_mutex r_mutex_t;
  typedef iresearch::async_utils::read_write_mutex::write_mutex w_mutex_t;

  // concurrent read lock
  {
    mutex_t mutex;
    r_mutex_t wrapper(mutex);
    std::lock_guard<r_mutex_t> lock(wrapper);

    ASSERT_FALSE(mutex.owns_write());

    std::thread thread([&wrapper]()->void{ std::unique_lock<r_mutex_t> lock(wrapper); ASSERT_TRUE(lock.owns_lock()); });
    thread.join();
  }

  // exclusive write lock
  {
    mutex_t mutex;
    w_mutex_t wrapper(mutex);
    std::lock_guard<w_mutex_t> lock(wrapper);

    ASSERT_TRUE(mutex.owns_write());
    ASSERT_TRUE(wrapper.owns_write());

    std::thread thread([&wrapper]()->void{ ASSERT_FALSE(wrapper.try_lock()); });
    thread.join();
  }

  // read block write
  {
    mutex_t mutex;
    r_mutex_t wrapper(mutex);
    std::lock_guard<r_mutex_t> lock(wrapper);

    std::thread thread([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
    thread.join();
  }

  // write block read
  {
    mutex_t mutex;
    w_mutex_t wrapper(mutex);
    std::lock_guard<w_mutex_t> lock(wrapper);

    std::thread thread([&mutex]()->void{ r_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
    thread.join();
  }

  // writer non-starvation (reader waits for pending writers)
  {
    std::condition_variable cond;
    std::mutex cond_mtx;
    std::mutex begin0, end0;
    std::mutex begin1, end1;
    std::mutex begin2, end2;
    std::unique_lock<std::mutex> cond_lock(cond_mtx);
    std::unique_lock<std::mutex> lock_begin0(begin0), lock_end0(end0);
    std::unique_lock<std::mutex> lock_begin1(begin1), lock_end1(end1);
    std::unique_lock<std::mutex> lock_begin2(begin2), lock_end2(end2);
    mutex_t mutex;

    std::thread thread0([&begin0, &end0, &cond, &mutex]()->void{
      std::unique_lock<std::mutex> lock_begin(begin0); // wait for start
      r_mutex_t wrapper(mutex);
      std::lock_guard<r_mutex_t> lock(wrapper);
      cond.notify_all(); // notify work done
      lock_begin.unlock();
      std::lock_guard<std::mutex> lock_end(end0); // wait for end
    });
    std::thread thread1([&begin1, &end1, &cond, &cond_mtx, &mutex]()->void{
      std::unique_lock<std::mutex> lock_begin(begin1); // wait for start
      w_mutex_t wrapper(mutex);
      std::lock_guard<w_mutex_t> lock(wrapper);
      {
        std::lock_guard<std::mutex> cond_lock(cond_mtx);
        cond.notify_all(); // notify work done
        lock_begin.unlock();
      }
      std::lock_guard<std::mutex> lock_end(end1); // wait for end
    });
    std::thread thread2([&begin2, &end2, &cond, &cond_mtx, &mutex]()->void{
      std::unique_lock<std::mutex> lock_begin(begin2); // wait for start
      r_mutex_t wrapper(mutex);
      std::lock_guard<r_mutex_t> lock(wrapper);
      {
        std::lock_guard<std::mutex> cond_lock(cond_mtx);
        cond.notify_all(); // notify work done
        lock_begin.unlock();
      }
      std::lock_guard<std::mutex> lock_end(end2); // wait for end
    });

    ASSERT_TRUE(std::cv_status::no_timeout == cond.wait_for(lock_begin0, std::chrono::milliseconds(1000))); // wait for 1st reader

    lock_begin1.unlock(); // start writer
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume writer blocks on lock within 100ms
    lock_begin2.unlock(); // start 2nd reader
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume 2nd reader blocks on lock within 100ms
    // assume writer registers with mutex before reader due to timeout above

    lock_end0.unlock(); // allow first reader to finish
    ASSERT_TRUE(std::cv_status::no_timeout == cond.wait_for(cond_lock, std::chrono::milliseconds(1000))); // wait for reader or writer to finish
    ASSERT_FALSE(lock_begin2.try_lock()); // reader should still be blocked
    ASSERT_TRUE(lock_begin1.try_lock()); // writer should have already unblocked

    cond_lock.unlock(); // allow remaining thread to notify condition
    lock_end1.unlock(); // allow writer thread to finish
    lock_end2.unlock(); // allow 2nd reader thread to finish
    thread0.join();
    thread1.join();
    thread2.join();
  }

  // reader downgrade to a reader blocks a new writer but allows concurrent reader
  {
    mutex_t mutex;
    r_mutex_t wrapper(mutex);
    std::lock_guard<r_mutex_t> lock(wrapper);

    ASSERT_FALSE(mutex.owns_write());
    mutex.unlock(true); // downgrade to a read lock
    ASSERT_FALSE(mutex.owns_write());

    // test write lock aquire fails (mutex is locked)
    std::thread w_thread([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
    w_thread.join();

    // test write lock aquire passes (mutex is not write-locked)
    std::thread r_thread([&wrapper]()->void{ std::unique_lock<r_mutex_t> lock(wrapper); ASSERT_TRUE(lock.owns_lock()); });
    r_thread.join();
  }

  // writer downgrade to a reader blocks a new writer but allows concurrent reader
  {
    mutex_t mutex;
    r_mutex_t r_wrapper(mutex);
    w_mutex_t w_wrapper(mutex);
    std::lock_guard<w_mutex_t> lock(w_wrapper);

    ASSERT_TRUE(mutex.owns_write());
    ASSERT_TRUE(w_wrapper.owns_write());
    w_wrapper.unlock(true); // downgrade to a read lock
    ASSERT_FALSE(mutex.owns_write());
    ASSERT_FALSE(w_wrapper.owns_write());

    // test write lock aquire fails (mutex is locked)
    std::thread w_thread([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
    w_thread.join();

    // test write lock aquire passes (mutex is not write-locked)
    std::thread r_thread([&r_wrapper]()->void{ std::unique_lock<r_mutex_t> lock(r_wrapper); ASSERT_TRUE(lock.owns_lock()); });
    r_thread.join();
  }

  // reader recursive lock
  {
    mutex_t mutex;
    r_mutex_t r_wrapper(mutex);
    w_mutex_t w_wrapper(mutex);

    {
      std::lock_guard<r_mutex_t> lock0(r_wrapper);

      // test write lock aquire fails (mutex is locked)
      std::thread thread0([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
      thread0.join();

      {
        std::unique_lock<r_mutex_t> lock1(r_wrapper, std::try_to_lock);
        ASSERT_TRUE(lock1.owns_lock());

        // test write lock aquire fails (mutex is locked)
        std::thread thread1([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
        thread1.join();
      }

      // test write lock aquire fails (mutex is locked)
      std::thread thread2([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
      thread2.join();
    }

    // test write lock aquire passes (mutex is not write-locked)
    std::thread thread3([&w_wrapper]()->void{ std::unique_lock<w_mutex_t> lock(w_wrapper, std::try_to_lock); ASSERT_TRUE(lock.owns_lock()); });
    thread3.join();
  }

  // writer recursive lock
  {
    mutex_t mutex;
    r_mutex_t r_wrapper(mutex);
    w_mutex_t w_wrapper(mutex);

    {
      std::lock_guard<w_mutex_t> lock0(w_wrapper);

      // test write lock aquire fails (mutex is locked)
      std::thread thread0([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
      thread0.join();

      {
        std::unique_lock<w_mutex_t> lock1(w_wrapper, std::try_to_lock);
        ASSERT_TRUE(lock1.owns_lock());

        // test read lock aquire fails (mutex is locked)
        std::thread thread1([&mutex]()->void{ r_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
        thread1.join();

        // test write lock aquire fails (mutex is locked)
        std::thread thread2([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
        thread2.join();

        w_wrapper.unlock(true); // downgrade recursion to read-only

        // test read lock aquire fails (mutex is locked)
        std::thread thread3([&mutex]()->void{ r_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
        thread3.join();

        // test write lock aquire fails (mutex is locked)
        std::thread thread4([&mutex]()->void{ w_mutex_t wrapper(mutex); ASSERT_FALSE(wrapper.try_lock()); });
        thread4.join();

        std::unique_lock<r_mutex_t> lock2(r_wrapper, std::try_to_lock);
        ASSERT_TRUE(lock2.owns_lock());
      }
    }

    // test write lock aquire passes (mutex is not write-locked)
    std::thread thread5([&w_wrapper]()->void{ std::unique_lock<w_mutex_t> lock(w_wrapper, std::try_to_lock); ASSERT_TRUE(lock.owns_lock()); });
    thread5.join();
  }

  // reader recursive with writer pending
  {
    mutex_t mutex;
    r_mutex_t r_wrapper(mutex);
    w_mutex_t w_wrapper(mutex);

    {
      std::unique_lock<r_mutex_t> lock0(r_wrapper);

      // write-pending
      std::thread thread0([&w_wrapper]()->void{ std::unique_lock<w_mutex_t> lock(w_wrapper); });
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume thread starts within 100msec

      {
        std::unique_lock<r_mutex_t> lock1(r_wrapper, std::try_to_lock);
        ASSERT_FALSE(lock1.owns_lock()); // cannot aquire recursive read-lock if write-lock pending (limitation)
      }

      lock0.unlock();
      thread0.join();
    }
  }

}

TEST_F(async_utils_tests, test_thread_pool_run_mt) {
  // test schedule 1 task
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::condition_variable cond;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    auto task = [&mutex, &cond]()->void { std::lock_guard<std::mutex> lock(mutex); cond.notify_all(); };

    pool.run(std::move(task));
    ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000)));
  }

  // test schedule 3 task sequential
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::condition_variable cond;
    notifying_counter count(cond, 3);
    std::mutex mutex;
    std::mutex sync_mutex;
    auto task1 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    auto task2 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    auto task3 = [&mutex, &sync_mutex, &count]()->void { { std::lock_guard<std::mutex> lock(mutex); } std::unique_lock<std::mutex> lock(sync_mutex, std::try_to_lock); if (lock.owns_lock()) ++count; std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    std::unique_lock<std::mutex> lock(mutex);

    pool.run(std::move(task1));
    pool.run(std::move(task2));
    pool.run(std::move(task3));
    ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000))); // wait for all 3 tasks
    pool.stop();
  }

  // test schedule 3 task parallel
  {
    iresearch::async_utils::thread_pool pool(3, 0);
    std::condition_variable cond;
    notifying_counter count(cond, 3);
    std::mutex mutex;
    auto task1 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    auto task3 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    std::unique_lock<std::mutex> lock(mutex);

    pool.run(std::move(task1));
    pool.run(std::move(task2));
    pool.run(std::move(task3));
    ASSERT_TRUE(count || std::cv_status::no_timeout == cond.wait_for(lock, std::chrono::milliseconds(1000)) || count); // wait for all 3 tasks
    lock.unlock();
    pool.stop();
  }

  // test schedule 1 task exception + 1 task
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::condition_variable cond;
    notifying_counter count(cond, 2);
    std::mutex mutex;
    auto task1 = [&mutex, &count]()->void { ++count; throw "error"; };
    auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    std::unique_lock<std::mutex> lock(mutex);
    std::mutex dummy_mutex;
    std::unique_lock<std::mutex> dummy_lock(dummy_mutex);

    pool.run(std::move(task1));
    pool.run(std::move(task2));
    ASSERT_TRUE(count || std::cv_status::no_timeout == cond.wait_for(dummy_lock, std::chrono::milliseconds(10000)) || count); // wait for all 2 tasks (exception trace is slow on MSVC and even slower on *NIX with gdb)
    ASSERT_EQ(1, pool.threads());
    lock.unlock();
    pool.stop(true);
  }
}

TEST_F(async_utils_tests, test_thread_pool_bound_mt) {
  // test max threads
  {
    iresearch::async_utils::thread_pool pool(0, 0);
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
    pool.max_threads(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads start within 100msec
    ASSERT_EQ(2, count); // 2 tasks started
    ASSERT_EQ(2, pool.threads());
    ASSERT_EQ(2, pool.tasks_active());
    ASSERT_EQ(1, pool.tasks_pending());
    lock.unlock();
    pool.stop(true);
  }

  // test max threads delta grow
  {
    iresearch::async_utils::thread_pool pool(0, 0);
    std::atomic<size_t> count(0);
    std::mutex mutex;
    auto task = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    std::unique_lock<std::mutex> lock(mutex);

    ASSERT_EQ(0, pool.threads());
    pool.run(std::move(task));
    pool.max_threads_delta(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads start within 100msec
    ASSERT_EQ(1, count); // 1 task started
    ASSERT_EQ(1, pool.threads());
    ASSERT_EQ(1, pool.tasks_active());
    ASSERT_EQ(0, pool.tasks_pending());
    lock.unlock();
    pool.stop(true);
  }

  // test max threads delta
  {
    iresearch::async_utils::thread_pool pool(1, 10);

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

  // test max idle
  {
    iresearch::async_utils::thread_pool pool(0, 0);
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
    pool.run(std::move(task1));
    pool.run(std::move(task2));
    pool.run(std::move(task3));
    pool.max_idle(1);
    pool.max_threads(3);
    ASSERT_TRUE(start_count || std::cv_status::no_timeout == start_cond.wait_for(start_lock, std::chrono::milliseconds(1000)) || start_count); // wait for all 3 tasks to start
    ASSERT_EQ(0, count); // 0 tasks complete
    ASSERT_EQ(3, pool.threads());
    ASSERT_EQ(3, pool.tasks_active());
    ASSERT_EQ(0, pool.tasks_pending());
    lock1.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads finish within 100msec
    ASSERT_EQ(2, count); // 2 tasks complete
    ASSERT_EQ(2, pool.threads());
    lock2.unlock();
    pool.stop(true);
  }

  // test max idle delta
  {
    iresearch::async_utils::thread_pool pool(10, 1);

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
}

TEST_F(async_utils_tests, test_thread_pool_stop_mt) {
  // test stop run pending
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::atomic<size_t> count(0);
    std::mutex mutex;
    auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    std::unique_lock<std::mutex> lock(mutex);

    pool.run(std::move(task1));
    pool.run(std::move(task2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads start within 100msec (2 threads)
    lock.unlock();
    pool.stop(); // blocking call (thread runtime duration simulated via sleep)
    ASSERT_EQ(2, count); // all tasks ran
  }

  // test stop skip pending
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::atomic<size_t> count(0);
    std::mutex mutex;
    auto task1 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    auto task2 = [&mutex, &count]()->void { ++count; { std::lock_guard<std::mutex> lock(mutex); } std::this_thread::sleep_for(std::chrono::milliseconds(300)); };
    std::unique_lock<std::mutex> lock(mutex);

    pool.run(std::move(task1));
    pool.run(std::move(task2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads start within 100msec (1 thread)
    lock.unlock();
    pool.stop(true); // blocking call (thread runtime duration simulated via sleep)
    ASSERT_EQ(1, count); // only 1 task ran
  }

  // test pool stop + run
  {
    iresearch::async_utils::thread_pool pool(1, 0);
    std::atomic<size_t> count(0);
    std::mutex mutex;
    auto task1 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    auto task2 = [&mutex, &count]()->void { ++count; std::lock_guard<std::mutex> lock(mutex); };
    std::unique_lock<std::mutex> lock(mutex);

    ASSERT_EQ(0, pool.threads());
    pool.run(std::move(task1));
    pool.max_threads(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // assume threads start within 100msec
    ASSERT_EQ(1, count); // 1 task started
    ASSERT_EQ(1, pool.threads());
    lock.unlock();
    pool.stop(true);
    ASSERT_FALSE(pool.run(std::move(task2)));
  }

  // test multiple calls to stop will all block
  {
    irs::async_utils::thread_pool pool(1, 0);
    std::condition_variable cond;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    auto task = [&mutex, &cond]()->void { std::lock_guard<std::mutex> lock(mutex); cond.notify_all(); };

    ASSERT_EQ(0, pool.threads());
    pool.run(std::move(task));
    ASSERT_EQ(1, pool.threads());

    std::condition_variable cond2;
    std::mutex mutex2;
    std::unique_lock<std::mutex> lock2(mutex2);
    std::atomic<bool> stop(false);
    std::thread thread1([&pool, &mutex2, &cond2, &stop]()->void { pool.stop(); stop = true; std::lock_guard<std::mutex> lock(mutex2); cond2.notify_all(); });
    std::thread thread2([&pool, &mutex2, &cond2, &stop]()->void { pool.stop(); stop = true; std::lock_guard<std::mutex> lock(mutex2); cond2.notify_all(); });

    auto result = cond.wait_for(lock2, std::chrono::milliseconds(1000)); // assume thread blocks in 1000ms

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request
    MSVC2015_ONLY(while(!stop && result == std::cv_status::no_timeout) result = cond2.wait_for(lock2, std::chrono::milliseconds(1000)));
    MSVC2017_ONLY(while(!stop && result == std::cv_status::no_timeout) result = cond2.wait_for(lock2, std::chrono::milliseconds(1000)));

    ASSERT_EQ(std::cv_status::timeout, result);
    // ^^^ expecting timeout because pool should block indefinitely
    lock2.unlock();
    ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000)));
    thread1.join();
    thread2.join();
  }

  // test stop with a single thread will stop threads
  {
    irs::async_utils::thread_pool pool(1, 1);

    pool.run([]()->void{}); // start a single thread
    ASSERT_EQ(1, pool.threads());
    pool.stop();
    ASSERT_EQ(0, pool.threads());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------