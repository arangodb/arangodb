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

#include "gtest/gtest.h"
#include "utils/object_pool.hpp"
#include "utils/thread_utils.hpp"

#include <array>

using namespace std::chrono_literals;

namespace tests {

struct test_slow_sobject {
  using ptr = std::shared_ptr<test_slow_sobject>;
  int id;
  test_slow_sobject (int i): id(i) {
    ++TOTAL_COUNT;
  }
  static std::atomic<size_t> TOTAL_COUNT; // # number of objects created
  static ptr make(int i) {
    std::this_thread::sleep_for(2s); // wait 1 sec to ensure index file timestamps differ
    return ptr(new test_slow_sobject(i));
  }
};

std::atomic<size_t> test_slow_sobject::TOTAL_COUNT{};

struct test_sobject {
  using ptr = std::shared_ptr<test_sobject>;
  int id;
  test_sobject(int i): id(i) { }
  static ptr make(int i) { return ptr(new test_sobject(i)); }
};

struct test_sobject_nullptr {
  using ptr = std::shared_ptr<test_sobject_nullptr>;
  static size_t make_count;
  static ptr make() { ++make_count; return nullptr; }
};

/*static*/ size_t test_sobject_nullptr::make_count = 0;

struct test_uobject {
  using ptr = std::unique_ptr<test_uobject>;
  int id;
  test_uobject(int i): id(i) {}
  static ptr make(int i) { return ptr(new test_uobject(i)); }
};

struct test_uobject_nullptr {
  using ptr = std::unique_ptr<test_uobject_nullptr>;
  static size_t make_count;
  static ptr make() { ++make_count; return nullptr; }
};

/*static*/ size_t test_uobject_nullptr::make_count = 0;

}

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(bounded_object_pool_tests, check_total_number_of_instances) {
  const size_t MAX_COUNT = 2;
  iresearch::bounded_object_pool<test_slow_sobject> pool(MAX_COUNT);

  std::mutex mutex;
  std::condition_variable ready_cv;
  bool ready{false};

  std::atomic<size_t> id{};
  test_slow_sobject::TOTAL_COUNT = 0;

  auto job = [&mutex, &ready_cv, &pool, &ready, &id](){
    // wait for all threads to be ready
    {
      auto lock = irs::make_unique_lock(mutex);

      while (!ready) {
        ready_cv.wait(lock);
      }
    }

    pool.emplace(id++);
  };

  auto job_shared = [&mutex, &ready_cv, &pool, &ready, &id](){
    // wait for all threads to be ready
    {
      auto lock = irs::make_unique_lock(mutex);

      while (!ready) {
        ready_cv.wait(lock);
      }
    }

    pool.emplace(id++).release();
  };

  const size_t THREADS_COUNT = 32;
  std::vector<std::thread> threads;

  for (size_t i = 0; i < THREADS_COUNT/2; ++i) {
    threads.emplace_back(job);
    threads.emplace_back(job_shared);
  }

  // ready
  ready = true;
  ready_cv.notify_all();

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_LE(test_slow_sobject::TOTAL_COUNT.load(), MAX_COUNT);
}

TEST(bounded_object_pool_tests, test_sobject_pool) {
  // block on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1).release();

    {
      auto lock = irs::make_unique_lock(mutex);
      std::atomic<bool> emplace(false);
      std::thread thread([&cond, &mutex, &pool, &emplace]()->void{
        auto obj = pool.emplace(2);
        emplace = true;
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });

      auto result = cond.wait_for(lock, 1000ms); // assume thread blocks in 1000ms

      // As declaration for wait_for contains "It may also be unblocked spuriously." for all platforms
      while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, 1000ms);

      ASSERT_EQ(std::cv_status::timeout, result);
      // ^^^ expecting timeout because pool should block indefinitely
      obj.reset();
      lock.unlock();
      thread.join();
    }
  }

  // null objects not considered part of pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    irs::bounded_object_pool<test_sobject_nullptr> pool(2);
    test_sobject_nullptr::make_count = 0;
    auto obj = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_sobject_nullptr::make_count);

    {
      auto lock = irs::make_unique_lock(mutex);
      std::atomic<bool> emplace(false);
      std::thread thread([&cond, &mutex, &pool, &emplace]()->void {
        auto obj = pool.emplace();
        ASSERT_FALSE(obj);
        ASSERT_EQ(2, test_sobject_nullptr::make_count);
        emplace = true;
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });

    ASSERT_TRUE(std::cv_status::no_timeout == cond.wait_for(lock, 100ms) || emplace);
    obj.reset();
    lock.unlock();
    thread.join();
    }
  }

  // test object reuse
  {
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    auto obj_shared = pool.emplace(2).release();
    ASSERT_EQ(1, obj_shared->id);
    ASSERT_EQ(obj_ptr, obj_shared.get());
  }

  // test visitation
  {
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    std::condition_variable cond;
    std::mutex mutex;
    auto lock = irs::make_unique_lock(mutex);
    std::atomic<bool> visit(false);
    std::thread thread([&cond, &mutex, &pool, &visit]()->void {
      auto visitor = [](test_sobject& obj)->bool { return true; };
      pool.visit(visitor);
      visit = true;
      auto lock = irs::make_unique_lock(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, 1000ms); // assume thread finishes in 1000ms

    // As declaration for wait_for contains "It may also be unblocked spuriously." for all platforms
    while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, 1000ms);

    obj.reset();
    ASSERT_FALSE(obj);

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::timeout, result); // check only after joining with thread to avoid early exit
    // ^^^ expecting timeout because pool should block indefinitely
  }
}

TEST(bounded_object_pool_tests, test_uobject_pool) {
  // block on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);

    {
      auto lock = irs::make_unique_lock(mutex);
      std::atomic<bool> emplace(false);
      std::thread thread([&cond, &mutex, &pool, &emplace]()->void{
        auto obj = pool.emplace(2);
        emplace = true;
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });

      auto result = cond.wait_for(lock, 1000ms); // assume thread blocks in 1000ms

      // As declaration for wait_for contains "It may also be unblocked spuriously." for all platforms
      while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, 1000ms);

      ASSERT_EQ(std::cv_status::timeout, result);
      // ^^^ expecting timeout because pool should block indefinitely
      obj.reset();
      obj.reset();
      lock.unlock();
      thread.join();
    }
  }

  // null objects not considered part of pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    irs::bounded_object_pool<test_uobject_nullptr> pool(2);
    test_uobject_nullptr::make_count = 0;
    auto obj = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_uobject_nullptr::make_count);

    {
      auto lock = irs::make_unique_lock(mutex);
      std::thread thread([&cond, &mutex, &pool]()->void {
        auto lock = irs::make_unique_lock(mutex);
        auto obj = pool.emplace();
        ASSERT_FALSE(obj);
        ASSERT_EQ(2, test_uobject_nullptr::make_count);
        cond.notify_all();
      });

    ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 100ms));
    obj.reset();
    lock.unlock();
    thread.join();
    }
  }

  // test object reuse
  {
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // test visitation
  {
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    std::condition_variable cond;
    std::mutex mutex;
    auto lock = irs::make_unique_lock(mutex);
    std::atomic<bool> visit(false);
    std::thread thread([&cond, &mutex, &pool, &visit]()->void {
      auto visitor = [](test_uobject&)->bool { return true; };
      pool.visit(visitor);
      visit = true;
      auto lock = irs::make_unique_lock(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, 1000ms); // assume thread finishes in 1000ms

    // As declaration for wait_for contains "It may also be unblocked spuriously." for all platforms
    while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, 1000ms);

    obj.reset();

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::timeout, result);
    // ^^^ expecting timeout because pool should block indefinitely
  }
}

TEST(unbounded_object_pool_tests, construct) {
  iresearch::unbounded_object_pool<test_sobject> pool(42);
  ASSERT_EQ(42, pool.size());
}

TEST(unbounded_object_pool_tests, test_sobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1).release();

    {
      auto lock = irs::make_unique_lock(mutex);
      std::thread thread([&cond, &mutex, &pool]()->void{
        auto obj = pool.emplace(2);
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms)); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }
  }

  // test empty pool
  {
    iresearch::unbounded_object_pool<test_sobject> pool;
    ASSERT_EQ(0, pool.size());
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    auto obj_shared = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj_shared));
    ASSERT_EQ(2, obj_shared->id);
  }

  // null objects not considered part of pool
  {
    irs::unbounded_object_pool<test_sobject_nullptr> pool(2);
    test_sobject_nullptr::make_count = 0;
    auto obj = pool.emplace();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_sobject_nullptr::make_count);
    obj.reset();
    auto obj_shared = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(2, test_sobject_nullptr::make_count);
    obj.reset();
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    auto obj_shared = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj_shared));
    ASSERT_EQ(1, obj_shared->id);
    ASSERT_EQ(obj_ptr, obj_shared.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj0 = pool.emplace(1);
    ASSERT_TRUE(obj0);
    auto obj1 = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj1));
    auto* obj0_ptr = obj0.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj0.reset(); // will be placed back in pool first
    ASSERT_FALSE(obj0);
    ASSERT_EQ(nullptr, obj0.get());
    obj1.reset(); // will push obj1 out of the pool
    ASSERT_FALSE(obj1);
    ASSERT_EQ(nullptr, obj1.get());

    auto obj2 = pool.emplace(3).release();
    ASSERT_TRUE(bool(obj2));
    auto obj3 = pool.emplace(4);
    ASSERT_TRUE(obj3);
    ASSERT_EQ(1, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj0_ptr, obj2.get());
    ASSERT_NE(obj0_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(); // will clear objects inside the pool only
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get()); // same object

    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    pool.clear(); // will clear objects inside the pool only
    obj = pool.emplace(3); // 'obj' should not be reused
    ASSERT_TRUE(obj);
    ASSERT_EQ(3, obj->id);
  }
}

TEST(unbounded_object_pool_tests, test_uobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1).release();

    {
      auto lock = irs::make_unique_lock(mutex);
      std::thread thread([&cond, &mutex, &pool]()->void{
        auto obj = pool.emplace(2);
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms)); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }
  }

  // null objects not considered part of pool
  {
    irs::unbounded_object_pool<test_uobject_nullptr> pool(2);
    test_uobject_nullptr::make_count = 0;
    auto obj = pool.emplace();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_uobject_nullptr::make_count);
    obj.reset();
    auto obj_shared = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(2, test_uobject_nullptr::make_count);
    obj.reset();
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    auto obj_shared = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj_shared));
    ASSERT_EQ(1, obj_shared->id);
    ASSERT_EQ(obj_ptr, obj_shared.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj0 = pool.emplace(1);
    ASSERT_TRUE(obj0);
    auto obj1 = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj1));
    auto* obj0_ptr = obj0.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj0.reset(); // will be placed back in pool first
    ASSERT_FALSE(obj0);
    ASSERT_EQ(nullptr, obj0.get());
    obj1.reset(); // will push obj1 out of the pool
    ASSERT_FALSE(obj1);
    ASSERT_EQ(nullptr, obj1.get());

    auto obj2 = pool.emplace(3).release();
    ASSERT_TRUE(bool(obj2));
    auto obj3 = pool.emplace(4);
    ASSERT_TRUE(obj3);
    ASSERT_EQ(1, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj0_ptr, obj2.get());
    ASSERT_NE(obj0_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(); // will clear objects inside the pool only
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get()); // same object

    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    pool.clear(); // will clear objects inside the pool only
    obj = pool.emplace(3); // 'obj' should not be reused
    ASSERT_TRUE(obj);
    ASSERT_EQ(3, obj->id);
  }
}

TEST(unbounded_object_pool_tests, control_object_move) {
  irs::unbounded_object_pool<test_sobject> pool(2);
  ASSERT_EQ(2, pool.size());

  // move constructor
  {
    auto moved = pool.emplace(1);
    ASSERT_TRUE(moved);
    ASSERT_NE(nullptr, moved.get());
    ASSERT_EQ(1, moved->id);

    auto obj = std::move(moved);
    ASSERT_FALSE(moved);
    ASSERT_EQ(nullptr, moved.get());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
  }

  // move assignment
  {
    auto moved = pool.emplace(1);
    ASSERT_TRUE(moved);
    ASSERT_NE(nullptr, moved.get());
    ASSERT_EQ(1, moved->id);
    auto* moved_ptr = moved.get();

    auto obj = pool.emplace(2);
    ASSERT_TRUE(obj);
    ASSERT_EQ(2, obj->id);

    obj = std::move(moved);
    ASSERT_TRUE(obj);
    ASSERT_FALSE(moved);
    ASSERT_EQ(nullptr, moved.get());
    ASSERT_EQ(obj.get(), moved_ptr);
    ASSERT_EQ(1, obj->id);
  }
}

TEST(unbounded_object_pool_volatile_tests, construct) {
  iresearch::unbounded_object_pool_volatile<test_sobject> pool(42);
  ASSERT_EQ(42, pool.size());
  ASSERT_EQ(0, pool.generation_size());
}

TEST(unbounded_object_pool_volatile_tests, move) {
  irs::unbounded_object_pool_volatile<test_sobject> moved(2);
  ASSERT_EQ(2, moved.size());
  ASSERT_EQ(0, moved.generation_size());

  auto obj0 = moved.emplace(1);
  ASSERT_EQ(1, moved.generation_size());
  ASSERT_TRUE(obj0);
  ASSERT_NE(nullptr, obj0.get());
  ASSERT_EQ(1, obj0->id);

  irs::unbounded_object_pool_volatile<test_sobject> pool(std::move(moved));
  ASSERT_EQ(2, moved.generation_size());
  ASSERT_EQ(2, pool.generation_size());

  auto obj1 = pool.emplace(2);
  ASSERT_EQ(3, pool.generation_size()); // +1 for moved
  ASSERT_EQ(3, moved.generation_size());
  ASSERT_TRUE(obj1);
  ASSERT_NE(nullptr, obj1.get());
  ASSERT_EQ(2, obj1->id);

  // insert via moved pool
  auto obj2 = moved.emplace(3);
  ASSERT_EQ(4, pool.generation_size());
  ASSERT_EQ(4, moved.generation_size());
  ASSERT_TRUE(obj2);
  ASSERT_NE(nullptr, obj2.get());
  ASSERT_EQ(3, obj2->id);
}

TEST(unbounded_object_pool_volatile_tests, control_object_move) {
  irs::unbounded_object_pool_volatile<test_sobject> pool(2);
  ASSERT_EQ(2, pool.size());
  ASSERT_EQ(0, pool.generation_size());

  // move constructor
  {
    auto moved = pool.emplace(1);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(moved);
    ASSERT_NE(nullptr, moved.get());
    ASSERT_EQ(1, moved->id);

    auto obj = std::move(moved);
    ASSERT_FALSE(moved);
    ASSERT_EQ(nullptr, moved.get());
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
  }

  // move assignment
  {
    auto moved = pool.emplace(1);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(moved);
    ASSERT_NE(nullptr, moved.get());
    ASSERT_EQ(1, moved->id);
    auto* moved_ptr = moved.get();

    auto obj = pool.emplace(2);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(2, obj->id);

    obj = std::move(moved);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_FALSE(moved);
    ASSERT_EQ(nullptr, moved.get());
    ASSERT_EQ(obj.get(), moved_ptr);
    ASSERT_EQ(1, obj->id);
  }

  ASSERT_EQ(0, pool.generation_size());
}

TEST(unbounded_object_pool_volatile_tests, test_sobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool_volatile<test_sobject> pool(1);
    ASSERT_EQ(0, pool.generation_size());
    auto obj = pool.emplace(1).release();
    ASSERT_EQ(1, pool.generation_size());

    {
      auto lock = irs::make_unique_lock(mutex);
      std::thread thread([&cond, &mutex, &pool]()->void{
        auto obj = pool.emplace(2);
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms)); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }

    ASSERT_EQ(1, pool.generation_size());
  }

  // test empty pool
  {
    iresearch::unbounded_object_pool_volatile<test_sobject> pool;
    ASSERT_EQ(0, pool.size());
    auto obj = pool.emplace(1);
    ASSERT_TRUE(obj);

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    auto obj_shared = pool.emplace(2).release();
    ASSERT_TRUE(bool(obj_shared));
    ASSERT_EQ(2, obj_shared->id);
  }

  // null objects not considered part of pool
  {
    irs::unbounded_object_pool_volatile<test_sobject_nullptr> pool(2);
    test_sobject_nullptr::make_count = 0;
    auto obj = pool.emplace();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_sobject_nullptr::make_count);
    obj.reset();
    auto obj_shared = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(2, test_sobject_nullptr::make_count);
    obj.reset();
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool_volatile<test_sobject> pool(1);
    ASSERT_EQ(0, pool.generation_size());
    auto obj = pool.emplace(1);
    ASSERT_EQ(1, pool.generation_size());
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    ASSERT_EQ(0, pool.generation_size());
    auto obj_shared = pool.emplace(2).release();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_EQ(1, obj_shared->id);
    ASSERT_EQ(obj_ptr, obj_shared.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool_volatile<test_sobject> pool(1);
    ASSERT_EQ(0, pool.generation_size());
    auto obj0 = pool.emplace(1);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj0);
    auto obj1 = pool.emplace(2).release();
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(bool(obj1));
    auto* obj0_ptr = obj0.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj0.reset(); // will be placed back in pool first
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj0);
    ASSERT_EQ(nullptr, obj0.get());
    obj1.reset(); // will push obj1 out of the pool
    ASSERT_EQ(0, pool.generation_size());
    ASSERT_FALSE(obj1);
    ASSERT_EQ(nullptr, obj1.get());

    auto obj2 = pool.emplace(3).release();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(bool(obj2));
    auto obj3 = pool.emplace(4);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj3);
    ASSERT_EQ(1, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj0_ptr, obj2.get());
    ASSERT_NE(obj0_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool_volatile<test_sobject> pool(1);
    ASSERT_EQ(0, pool.generation_size());
    auto obj_noreuse = pool.emplace(-1);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj_noreuse);
    auto obj = pool.emplace(1);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(); // clear existing in a pool
    ASSERT_EQ(2, pool.generation_size());
    obj.reset();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2); // may return same memory address as obj_ptr, but constructor would have been called
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(true); // clear existing in a pool and prevent external object from returning back
    ASSERT_EQ(0, pool.generation_size());
    obj.reset();
    ASSERT_EQ(0, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2); // may return same memory address as obj_ptr, but constructor would have been called
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(2, obj->id);

    obj_noreuse.reset(); // reset value from previuos generation
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj_noreuse);
    ASSERT_EQ(nullptr, obj_noreuse.get());
    obj = pool.emplace(3); // 'obj_noreuse' should not be reused
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_EQ(3, obj->id);
  }
}

TEST(unbounded_object_pool_volatile_tests, return_object_after_pool_destroyed) {
  auto pool = irs::memory::make_unique<irs::unbounded_object_pool_volatile<test_sobject>>(1);
  ASSERT_EQ(0, pool->generation_size());
  ASSERT_NE(nullptr, pool);

  auto obj = pool->emplace(42);
  ASSERT_EQ(1, pool->generation_size());
  ASSERT_TRUE(obj);
  ASSERT_EQ(42, obj->id);
  auto objShared = pool->emplace(442).release();
  ASSERT_EQ(2, pool->generation_size());
  ASSERT_NE(nullptr, objShared);
  ASSERT_EQ(442, objShared->id);

  // destroy pool
  pool.reset();
  ASSERT_EQ(nullptr, pool);

  // ensure objects are still there
  ASSERT_EQ(42, obj->id);
  ASSERT_EQ(442, objShared->id);
}

TEST(unbounded_object_pool_volatile_tests, test_uobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool_volatile<test_uobject> pool(1);
    auto obj = pool.emplace(1);

    {
      auto lock = irs::make_unique_lock(mutex);
      std::thread thread([&cond, &mutex, &pool]()->void{
        auto obj = pool.emplace(2);
        auto lock = irs::make_unique_lock(mutex);
        cond.notify_all();
      });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, 1000ms)); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }
  }

  // null objects not considered part of pool
  {
    irs::unbounded_object_pool_volatile<test_uobject_nullptr> pool(2);
    test_uobject_nullptr::make_count = 0;
    auto obj = pool.emplace();
    ASSERT_FALSE(obj);
    ASSERT_EQ(1, test_uobject_nullptr::make_count);
    obj.reset();
    auto obj_shared = pool.emplace().release();
    ASSERT_FALSE(obj);
    ASSERT_EQ(2, test_uobject_nullptr::make_count);
    obj.reset();
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool_volatile<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool_volatile<test_uobject> pool(1);
    auto obj0 = pool.emplace(1);
    auto obj1 = pool.emplace(2);
    auto* obj0_ptr = obj0.get();
    auto* obj1_ptr = obj1.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj1.reset(); // will be placed back in pool first
    ASSERT_FALSE(obj1);
    ASSERT_EQ(nullptr, obj1.get());
    obj0.reset(); // will push obj1 out of the pool
    ASSERT_FALSE(obj0);
    ASSERT_EQ(nullptr, obj0.get());

    auto obj2 = pool.emplace(3);
    auto obj3 = pool.emplace(4);
    ASSERT_EQ(2, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj1_ptr, obj2.get());
    ASSERT_NE(obj1_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool_volatile<test_uobject> pool(1);
    ASSERT_EQ(0, pool.generation_size());
    auto obj_noreuse = pool.emplace(-1);
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj_noreuse);
    auto obj = pool.emplace(1);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2);
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(); // clear existing in a pool
    ASSERT_EQ(2, pool.generation_size());
    obj.reset();
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2); // may return same memory address as obj_ptr, but constructor would have been called
    ASSERT_EQ(2, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear(true); // clear existing in a pool and prevent external object from returning back
    ASSERT_EQ(0, pool.generation_size());
    obj.reset();
    ASSERT_EQ(0, pool.generation_size());
    ASSERT_FALSE(obj);
    ASSERT_EQ(nullptr, obj.get());
    obj = pool.emplace(2); // may return same memory address as obj_ptr, but constructor would have been called
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_TRUE(obj);
    ASSERT_EQ(2, obj->id);

    obj_noreuse.reset(); // reset value from previuos generation
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_FALSE(obj_noreuse);
    ASSERT_EQ(nullptr, obj_noreuse.get());
    obj = pool.emplace(3); // 'obj_noreuse' should not be reused
    ASSERT_EQ(1, pool.generation_size());
    ASSERT_EQ(3, obj->id);
  }
}

TEST(concurrent_linked_list_test, push_pop) {
  typedef irs::concurrent_stack<size_t> stack;
  typedef stack::node_type node_type;

  std::vector<node_type> nodes(10);

  size_t v = 0;
  for (auto& node : nodes) {
    node.value = v++;
  }

  stack list;
  ASSERT_TRUE(list.empty());
  ASSERT_EQ(nullptr, list.pop());

  for (auto& node : nodes) {
    list.push(node);
  }
  ASSERT_FALSE(list.empty());

  node_type* node = 0;
  auto rbegin = nodes.rbegin();
  while ((node = list.pop())) {
    ASSERT_EQ(&*rbegin, node);
    list.push(*node);
    node = list.pop();
    ASSERT_EQ(&*rbegin, node);
    ++rbegin;
  }
  ASSERT_EQ(nodes.rend(), rbegin);
}

TEST(concurrent_linked_list_test, push) {
  typedef irs::concurrent_stack<size_t> stack;
  typedef stack::node_type node_type;

  std::vector<node_type> nodes(10);
  stack list;
  ASSERT_TRUE(list.empty());
  ASSERT_EQ(nullptr, list.pop());

  for (auto& node : nodes) {
    list.push(node);
  }
  ASSERT_FALSE(list.empty());

  size_t count = 0;
  while (auto* head = list.pop()) {
    ++count;
  }
  ASSERT_TRUE(list.empty());

  ASSERT_EQ(nodes.size(), count);
}

TEST(concurrent_linked_list_test, move) {
  typedef irs::concurrent_stack<size_t> stack;

  std::array<stack::node_type, 10> nodes;
  stack moved;
  ASSERT_TRUE(moved.empty());

  size_t i = 0;
  for (auto& node : nodes) {
    node.value = i;
    moved.push(node);
  }
  ASSERT_FALSE(moved.empty());

  stack list(std::move(moved));
  ASSERT_TRUE(moved.empty());
  ASSERT_FALSE(list.empty());

  auto rbegin = nodes.rbegin();
  while (auto* node = list.pop()) {
    ASSERT_EQ(rbegin->value, node->value);
    ++rbegin;
  }

  ASSERT_EQ(nodes.rend(), rbegin);
  ASSERT_TRUE(list.empty());
}

TEST(concurrent_linked_list_test, move_assignment) {
  typedef irs::concurrent_stack<size_t> stack;

  std::array<stack::node_type, 10> nodes;
  stack list0;
  ASSERT_TRUE(list0.empty());
  stack list1;
  ASSERT_TRUE(list1.empty());

  size_t i = 0;

  for (; i < nodes.size()/2; ++i) {
    auto& node = nodes[i];
    node.value = i;
    list0.push(node);
  }
  ASSERT_FALSE(list0.empty());

  for (; i < nodes.size(); ++i) {
    auto& node = nodes[i];
    node.value = i;
    list1.push(node);
  }
  ASSERT_FALSE(list1.empty());

  list0 = std::move(list1);

  auto rbegin = nodes.rbegin();
  while (auto* node = list0.pop()) {
    ASSERT_EQ(rbegin->value, node->value);
    ++rbegin;
  }
  ASSERT_TRUE(list0.empty());
  ASSERT_TRUE(list1.empty());
}

TEST(concurrent_linked_list_test, concurrent_pop) {
  const size_t NODES = 10000;
  const size_t THREADS = 16;

  typedef irs::concurrent_stack<size_t> stack;
  std::vector<stack::node_type> nodes(NODES);

  // build-up a list
  stack list;
  ASSERT_TRUE(list.empty());
  size_t size = 0;
  for (auto& node : nodes) {
    node.value = size++;
    list.push(node);
  }
  ASSERT_FALSE(list.empty());

  std::vector<std::vector<size_t>> threads_data(THREADS);
  std::vector<std::thread> threads;
  threads.reserve(THREADS);

  std::mutex mutex;
  std::condition_variable ready_cv;
  bool ready = false;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::remove_reference<decltype(mutex)>::type> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  // start threads
  {
    auto lock = irs::make_unique_lock(mutex);
    for (size_t i = 0; i < threads_data.size(); ++i) {
      auto& thread_data = threads_data[i];
      threads.emplace_back([&list, &wait_for_all, &thread_data]() {
        wait_for_all();

        while (auto* head = list.pop()) {
          thread_data.push_back(head->value);
        }
      });
    }
  }

  // all threads are registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_TRUE(list.empty());

  std::set<size_t> results;
  for (auto& thread_data : threads_data) {
    for (auto value : thread_data) {
      ASSERT_TRUE(results.insert(value).second);
    }
  }
  ASSERT_EQ(NODES, results.size());
}

TEST(concurrent_linked_list_test, concurrent_push) {
  const size_t NODES = 10000;
  const size_t THREADS = 16;

  typedef irs::concurrent_stack<size_t> stack;

  // build-up a list
  stack list;
  ASSERT_TRUE(list.empty());

  std::vector<std::vector<stack::node_type>> threads_data(
    THREADS, std::vector<stack::node_type>{ NODES }
  );
  std::vector<std::thread> threads;
  threads.reserve(THREADS);

  std::mutex mutex;
  std::condition_variable ready_cv;
  bool ready = false;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    auto lock = irs::make_unique_lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  // start threads
  {
    auto lock = irs::make_unique_lock(mutex);
    for (size_t i = 0; i < threads_data.size(); ++i) {
      auto& thread_data = threads_data[i];
      threads.emplace_back([&list, &wait_for_all, &thread_data]() {
        wait_for_all();

        size_t idx = 0;
        for (auto& node : thread_data) {
          node.value = idx++;
          list.push(node);
        }
      });
    }
  }

  // all threads are registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_FALSE(list.empty());

  std::vector<size_t> results(NODES);

  while (auto* node = list.pop()) {
    ASSERT_TRUE(node->value < results.size());
    ++results[node->value];
  }

  ASSERT_TRUE(
    results.front() == THREADS
    && irs::irstd::all_equal(results.begin(), results.end())
  );
}

TEST(concurrent_linked_list_test, concurrent_pop_push) {
  const size_t NODES = 10000;
  const size_t THREADS = 16;

  struct data {
    bool visited{};
    size_t num_owners{};
    size_t value{};
  };

  typedef irs::concurrent_stack<data> stack;
  std::vector<stack::node_type> nodes(NODES);

  // build-up a list
  stack list;
  ASSERT_TRUE(list.empty());
  for (auto& node : nodes) {
    list.push(node);
  }
  ASSERT_FALSE(list.empty());

  std::vector<std::thread> threads;
  threads.reserve(THREADS);

  std::mutex mutex;
  std::condition_variable ready_cv;
  bool ready = false;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    auto lock = irs::make_unique_lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  // start threads
  {
    auto lock = irs::make_unique_lock(mutex);
    for (size_t i = 0; i < THREADS; ++i) {
      threads.emplace_back([NODES, &list, &wait_for_all]() {
        wait_for_all();

        // no more than NODES
        size_t processed = 0;

        while (auto* head = list.pop()) {
          ++processed;
          EXPECT_LE(processed, 2*NODES);

          auto& value = head->value;

          if (!value.visited) {
            ++value.num_owners;
            ++value.value;
            value.visited = true;
            list.push(*head);
          } else {
            --value.num_owners;
            ASSERT_EQ(0, value.num_owners);
          }
        }
      });
    }
  }

  // all threads are registered... go, go, go...
  {
    auto lock = irs::make_unique_lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_TRUE(list.empty());

  size_t i =0;
  for (auto& node : nodes) {
    ASSERT_EQ(1, node.value.value);
    ASSERT_EQ(true, node.value.visited);
    ASSERT_EQ(0, node.value.num_owners);
  }
}
