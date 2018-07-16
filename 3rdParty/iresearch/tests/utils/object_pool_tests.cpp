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

NS_BEGIN(tests)

struct test_slow_sobject {
  DECLARE_SPTR(test_slow_sobject);
  int id;
  test_slow_sobject (int i): id(i) {
    ++TOTAL_COUNT;
  }
  static std::atomic<size_t> TOTAL_COUNT; // # number of objects created
  static ptr make(int i) {
    irs::sleep_ms(2000);
    return ptr(new test_slow_sobject(i));
  }
};

std::atomic<size_t> test_slow_sobject::TOTAL_COUNT{};

struct test_sobject {
  DECLARE_SPTR(test_sobject);
  int id;
  test_sobject(int i): id(i) { }
  static ptr make(int i) { return ptr(new test_sobject(i)); }
};

struct test_uobject {
  DECLARE_PTR(test_uobject);
  int id;
  test_uobject(int i): id(i) {}
  static ptr make(int i) { return ptr(new test_uobject(i)); }
};

class object_pool_tests: public ::testing::Test {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).
  }
};

NS_END

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(object_pool_tests, check_total_number_of_instances) {
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
      SCOPED_LOCK_NAMED(mutex, lock);

      while (!ready) {
        ready_cv.wait(lock);
      }
    }

    pool.emplace(id++);
  };

  const size_t THREADS_COUNT = 32;
  std::vector<std::thread> threads;

  for (size_t i = 0; i < THREADS_COUNT; ++i) {
    threads.emplace_back(job);
  }

  // ready
  ready = true;
  ready_cv.notify_all();

  for (auto& thread : threads) {
    thread.join();
  }

  ASSERT_LE(test_slow_sobject::TOTAL_COUNT.load(), MAX_COUNT);
}

TEST_F(object_pool_tests, bounded_sobject_pool) {
  // block on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);

    {
      SCOPED_LOCK_NAMED(mutex, lock);
      std::atomic<bool> emplace(false);
      std::thread thread([&cond, &mutex, &pool, &emplace]()->void{ auto obj = pool.emplace(2); emplace = true; SCOPED_LOCK(mutex); cond.notify_all(); });

      auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread blocks in 1000ms

      // MSVC 2015/2017 optimized code seems to sporadically notify condition variables without explicit request
      MSVC2015_OPTIMIZED_ONLY(while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));
      MSVC2017_ONLY(while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));

      ASSERT_EQ(std::cv_status::timeout, result);
      // ^^^ expecting timeout because pool should block indefinitely
      obj.reset();
      lock.unlock();
      thread.join();
    }
  }

  // test object reuse
  {
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // test shared visitation
  {
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);
    std::thread thread([&cond, &mutex, &pool]()->void {
      auto visitor = [](test_sobject& obj)->bool { return true; };
      pool.visit(visitor, true);
      SCOPED_LOCK(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread finishes in 1000ms

    obj.reset();

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::no_timeout, result); // check only after joining with thread to avoid early exit
  }

  // test exclusive visitation
  {
    iresearch::bounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);
    std::atomic<bool> visit(false);
    std::thread thread([&cond, &mutex, &pool, &visit]()->void {
      auto visitor = [](test_sobject& obj)->bool { return true; };
      pool.visit(visitor, false);
      visit = true;
      SCOPED_LOCK(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread finishes in 1000ms

    // MSVC 2015/2017 optimized code seems to sporadically notify condition variables without explicit request
    MSVC2015_OPTIMIZED_ONLY(while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));
    MSVC2017_ONLY(while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));

    obj.reset();

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::timeout, result); // check only after joining with thread to avoid early exit
    // ^^^ expecting timeout because pool should block indefinitely
  }
}

TEST_F(object_pool_tests, bounded_uobject_pool) {
  // block on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);

    {
      SCOPED_LOCK_NAMED(mutex, lock);
      std::atomic<bool> emplace(false);
      std::thread thread([&cond, &mutex, &pool, &emplace]()->void{ auto obj = pool.emplace(2); emplace = true; SCOPED_LOCK(mutex); cond.notify_all(); });

      auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread blocks in 1000ms

      // MSVC 2015/2017 optimized code seems to sporadically notify condition variables without explicit request
      MSVC2015_OPTIMIZED_ONLY(while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));
      MSVC2017_ONLY(while(!emplace && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));

      ASSERT_EQ(std::cv_status::timeout, result);
      // ^^^ expecting timeout because pool should block indefinitely
      obj.reset();
      obj.reset();
      lock.unlock();
      thread.join();
    }
  }

  // test object reuse
  {
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // test shared visitation
  {
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);
    std::thread thread([&cond, &mutex, &pool]()->void {
      auto visitor = [](test_uobject& obj)->bool { return true; };
      pool.visit(visitor, true);
      SCOPED_LOCK(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread finishes in 1000ms

    obj.reset();

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::no_timeout, result); // check only after joining with thread to avoid early exit
  }

  // test exclusive visitation
  {
    iresearch::bounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);
    std::atomic<bool> visit(false);
    std::thread thread([&cond, &mutex, &pool, &visit]()->void {
      auto visitor = [](test_uobject& obj)->bool { return true; };
      pool.visit(visitor, false);
      visit = true;
      SCOPED_LOCK(mutex);
      cond.notify_all();
    });
    auto result = cond.wait_for(lock, std::chrono::milliseconds(1000)); // assume thread finishes in 1000ms

    // MSVC 2015/2017 optimized code seems to sporadically notify condition variables without explicit request
    MSVC2015_OPTIMIZED_ONLY(while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));
    MSVC2017_ONLY(while(!visit && result == std::cv_status::no_timeout) result = cond.wait_for(lock, std::chrono::milliseconds(1000)));

    obj.reset();

    if (lock) {
      lock.unlock();
    }

    thread.join();
    ASSERT_EQ(std::cv_status::timeout, result);
    // ^^^ expecting timeout because pool should block indefinitely
  }
}

TEST_F(object_pool_tests, unbounded_sobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);

    {
      SCOPED_LOCK_NAMED(mutex, lock);
      std::thread thread([&cond, &mutex, &pool]()->void{ auto obj = pool.emplace(2); SCOPED_LOCK(mutex); cond.notify_all(); });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000))); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj0 = pool.emplace(1);
    auto obj1 = pool.emplace(2);
    auto* obj0_ptr = obj0.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj1.reset(); // will be placed back in pool first
    obj0.reset(); // will push obj1 out of the pool

    auto obj2 = pool.emplace(3);
    auto obj3 = pool.emplace(4);
    ASSERT_EQ(1, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj0_ptr, obj2.get());
    ASSERT_NE(obj0_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool<test_sobject> pool(1);
    auto obj_noreuse = pool.emplace(-1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear();
    obj.reset();
    obj = pool.emplace(2); // may return same memory address as obj_ptr, but constructor would have been called
    ASSERT_EQ(2, obj->id);

    obj_noreuse.reset();
    obj = pool.emplace(3); // 'obj_noreuse' should not be reused
    ASSERT_EQ(3, obj->id);
  }
}

TEST_F(object_pool_tests, unbounded_uobject_pool) {
  // create new untracked object on full pool
  {
    std::condition_variable cond;
    std::mutex mutex;
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);

    {
      SCOPED_LOCK_NAMED(mutex, lock);
      std::thread thread([&cond, &mutex, &pool]()->void{ auto obj = pool.emplace(2); SCOPED_LOCK(mutex); cond.notify_all(); });
      ASSERT_EQ(std::cv_status::no_timeout, cond.wait_for(lock, std::chrono::milliseconds(1000))); // assume threads start within 1000msec
      lock.unlock();
      thread.join();
    }
  }

  // test object reuse
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());
  }

  // ensure untracked object is not placed back in the pool
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj0 = pool.emplace(1);
    auto obj1 = pool.emplace(2);
    auto* obj0_ptr = obj0.get();

    ASSERT_EQ(1, obj0->id);
    ASSERT_EQ(2, obj1->id);
    ASSERT_NE(obj0_ptr, obj1.get());
    obj1.reset(); // will be placed back in pool first
    obj0.reset(); // will push obj1 out of the pool

    auto obj2 = pool.emplace(3);
    auto obj3 = pool.emplace(4);
    ASSERT_EQ(1, obj2->id);
    ASSERT_EQ(4, obj3->id);
    ASSERT_EQ(obj0_ptr, obj2.get());
    ASSERT_NE(obj0_ptr, obj3.get());
    // obj3 may have been allocated in the same addr as obj1, so can't safely validate
  }

  // test pool clear
  {
    iresearch::unbounded_object_pool<test_uobject> pool(1);
    auto obj_noreuse = pool.emplace(-1);
    auto obj = pool.emplace(1);
    auto* obj_ptr = obj.get();

    ASSERT_EQ(1, obj->id);
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(1, obj->id);
    ASSERT_EQ(obj_ptr, obj.get());

    pool.clear();
    obj.reset();
    obj = pool.emplace(2);
    ASSERT_EQ(2, obj->id); // may return same memory address as obj_ptr, but constructor would have been called

    obj_noreuse.reset();
    obj = pool.emplace(3); // 'obj_noreuse' should not be reused
    ASSERT_EQ(3, obj->id);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------