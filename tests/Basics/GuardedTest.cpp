////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <Mocks/Death_Test.h>

#include "Basics/Guarded.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"

#include <Basics/UnshackledMutex.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace arangodb::tests {

struct UnderGuard {
  int val{};
};
struct UnderGuardAtomic {
  std::atomic<int> val{};
};

template <typename T>
class GuardedTest : public ::testing::Test {
 protected:
  using Mutex = typename T::first_type;
  template <typename M>
  using Lock = typename T::second_type::template instantiate<M>;
  template <typename V>
  using Guarded = Guarded<V, Mutex, Lock>;
};

TYPED_TEST_CASE_P(GuardedTest);

// Test helper that acquires a lock;
// then executes a callback that tries to acquire the same lock;
// then release the lock.
template <typename GuardedType, typename F>
static auto testWaitForLock(F&& callback) {
  auto guardedObj = GuardedType{1};
  std::atomic<bool> holds_lock = true;
  std::atomic<bool> waiting = false;

  // get a lock
  auto boxedGuard = std::make_unique<typename GuardedType::mutex_guard_type>(
      guardedObj.getLockedGuard());
  // start the thread tries to access the value, but needs to wait for the lock
  auto thread = std::thread(callback, &guardedObj, &holds_lock, &waiting);
  using namespace std::chrono_literals;
  while (!waiting.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(1us);
  }
  // give the thread a little time
  std::this_thread::sleep_for(1ms);

  // now release the lock
  holds_lock.store(false, std::memory_order_release);
  boxedGuard.reset();

  // the thread should now finish quickly
  thread.join();
}

TYPED_TEST_P(GuardedTest, test_copy_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  auto const value = guardedObj.copy();
  EXPECT_EQ(1, value.val);
  EXPECT_EQ(1, guardedObj.copy().val);
}

TYPED_TEST_P(GuardedTest, test_copy_waits_for_access) {
  using GuardedType = typename TestFixture::template Guarded<UnderGuard>;

  auto const copyValue = [](GuardedType* guardedObj,
                            std::atomic<bool>* holds_lock, std::atomic<bool>* waiting) {
    ASSERT_TRUE(holds_lock->load(std::memory_order_acquire));
    waiting->store(true, std::memory_order_release);
    auto v = guardedObj->copy();
    ASSERT_TRUE(!holds_lock->load(std::memory_order_acquire));
    ASSERT_EQ(1, v.val);
  };

  testWaitForLock<GuardedType>(copyValue);
}

TYPED_TEST_P(GuardedTest, test_assign_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  EXPECT_EQ(1, guardedObj.copy().val);
  // move assignment
  guardedObj.assign(UnderGuard{2});
  EXPECT_EQ(2, guardedObj.copy().val);
  // copy assignment
  auto const val = UnderGuard{3};
  guardedObj.assign(val);
  EXPECT_EQ(3, guardedObj.copy().val);
}

TYPED_TEST_P(GuardedTest, test_assign_waits_for_access) {
  using GuardedType = typename TestFixture::template Guarded<UnderGuard>;

  auto const assignValue = [](GuardedType* guardedObj,
                            std::atomic<bool>* holds_lock, std::atomic<bool>* waiting) {
    ASSERT_TRUE(holds_lock->load(std::memory_order_acquire));
    waiting->store(true, std::memory_order_release);
    guardedObj->assign(UnderGuard{2});
    ASSERT_TRUE(!holds_lock->load(std::memory_order_acquire));
    ASSERT_EQ(2, guardedObj->copy().val);
  };

  testWaitForLock<GuardedType>(assignValue);
}

TYPED_TEST_P(GuardedTest, test_guard_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  EXPECT_EQ(1, guardedObj.copy().val);
  {
    auto guard = guardedObj.getLockedGuard();
    EXPECT_EQ(1, guard.get().val);
    guard.get().val = 2;
    EXPECT_EQ(2, guard.get().val);
  }
  EXPECT_EQ(2, guardedObj.copy().val);
}

TYPED_TEST_P(GuardedTest, test_guard_waits_for_access) {
  using GuardedType = typename TestFixture::template Guarded<UnderGuard>;

  auto const acquireGuard = [](GuardedType* guardedObj,
                              std::atomic<bool>* holds_lock, std::atomic<bool>* waiting) {
    ASSERT_TRUE(holds_lock->load(std::memory_order_acquire));
    waiting->store(true, std::memory_order_release);
    {
      auto guard = guardedObj->getLockedGuard();
      ASSERT_TRUE(!holds_lock->load(std::memory_order_acquire));
      ASSERT_EQ(1, guard.get().val);
    }
  };

  testWaitForLock<GuardedType>(acquireGuard);
}

TYPED_TEST_P(GuardedTest, test_guard_unlock_releases_mutex) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  EXPECT_EQ(1, guardedObj.copy().val);
  auto guard = guardedObj.getLockedGuard();
  EXPECT_EQ(1, guard.get().val);
  guard.get().val = 2;
  EXPECT_EQ(2, guard.get().val);
  guard.unlock();

  auto threadStarted = std::atomic<bool>{false};
  auto couldAcquireLock = std::atomic<bool>{false};
  auto thr = std::thread([&] {
    threadStarted.store(true, std::memory_order_release);
    guardedObj.doUnderLock([&](auto&&) {
      couldAcquireLock.store(true, std::memory_order_release);
    });
  });

  EXPECT_EQ(2, guardedObj.copy().val);
  while (!threadStarted.load(std::memory_order_acquire)) {
    // busy wait
  }
  // wait generously for the thread to try to get the lock and do something
  std::this_thread::sleep_for(std::chrono::milliseconds{1});
  EXPECT_TRUE(couldAcquireLock.load(std::memory_order_acquire));
  thr.join();
}

TYPED_TEST_P(GuardedTest, test_guard_unlock_releases_value) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  EXPECT_EQ(1, guardedObj.copy().val);
  auto guard = guardedObj.getLockedGuard();
  EXPECT_EQ(1, guard->val);
  // make sure .get() and .operator->() behave the same
  EXPECT_EQ(&guard.get().val, &guard->val);
  EXPECT_EQ(&guard.get(), guard.operator->());
  guard.get().val = 2;
  EXPECT_EQ(2, guard->val);
  guard.unlock();

  // We cannot test guard.get(), as a reference bound to a dereferenced null
  // pointer is UB. But combined with the above check that they return the same
  // non-null pointer above, this should be good enough.
  ASSERT_EQ(nullptr, guard.operator->());
}

TYPED_TEST_P(GuardedTest, test_do_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  {
    bool didExecute = false;
    guardedObj.doUnderLock([&didExecute](UnderGuard& obj) {
      EXPECT_EQ(1, obj.val);
      obj.val = 2;
      didExecute = true;
      EXPECT_EQ(2, obj.val);
    });
    EXPECT_TRUE(didExecute);
    {
      auto guard = guardedObj.getLockedGuard();
      EXPECT_EQ(2, guard.get().val);
    }
  }
}

TYPED_TEST_P(GuardedTest, test_do_waits_for_access) {
  // Get a lock first, then make sure that doUnderLock() waits
  auto guardedObj = typename TestFixture::template Guarded<UnderGuardAtomic>{1};
  {
    auto thr = std::thread{};
    auto threadStarted = std::atomic<bool>{false};
    auto threadFinished = std::atomic<bool>{false};
    {
      auto guard = guardedObj.getLockedGuard();
      thr = std::thread([&] {
        threadStarted.store(true, std::memory_order_release);
        bool didExecute = false;
        auto const res = guardedObj.doUnderLock(
            [&didExecute](UnderGuardAtomic& obj) -> std::optional<std::monostate> {
              EXPECT_EQ(1, obj.val.load(std::memory_order_relaxed));
              obj.val.store(2, std::memory_order_release);
              didExecute = true;
              EXPECT_EQ(2, obj.val.load(std::memory_order_relaxed));
              return std::monostate{};
            });
        static_assert(std::is_same_v<std::optional<std::monostate> const, decltype(res)>);
        EXPECT_TRUE(res.has_value());
        EXPECT_TRUE(didExecute);
        threadFinished.store(true, std::memory_order_release);
      });

      while (!threadStarted.load(std::memory_order_acquire)) {
        // busy wait
      }
      // wait generously for the thread to try to get the lock and do something
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
      EXPECT_EQ(1, guard->val.load(std::memory_order_relaxed));
      EXPECT_FALSE(threadFinished.load(std::memory_order_acquire));
      // guard is destroyed at the end of the scope, so the lock is freed and
      // the thread should now be able to finish
    }

    thr.join();
    EXPECT_TRUE(threadStarted.load());
    EXPECT_TRUE(threadFinished.load());
    auto const val =
        guardedObj.doUnderLock([](auto const& obj) { return obj.val.load(); });
    EXPECT_EQ(val, 2);
  }
}

TYPED_TEST_P(GuardedTest, test_try_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  {
    // try is allowed to spuriously fail for no reason. But we expect it to
    // succeed at some point when noone holds the lock.
    bool didExecute = false;
    while (!didExecute) {
      auto const res = guardedObj.tryUnderLock([&didExecute](UnderGuard& obj) {
        EXPECT_EQ(1, obj.val);
        obj.val = 2;
        didExecute = true;
        EXPECT_EQ(2, obj.val);
        return;
      });
      static_assert(std::is_same_v<std::optional<std::monostate> const, decltype(res)>);
      EXPECT_EQ(didExecute, res.has_value());
      {
        auto guard = guardedObj.getLockedGuard();
        if (didExecute) {
          EXPECT_EQ(guard->val, 2);
        } else {
          EXPECT_EQ(guard->val, 1);
        }
      }
    }
  }
}

TYPED_TEST_P(GuardedTest, test_try_guard_allows_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  {
    // try is allowed to spuriously fail for no reason. But we expect it to
    // succeed at some point when noone holds the lock.
    bool didExecute = false;
    while (!didExecute) {
      if (auto guard = guardedObj.tryLockedGuard()) {
        EXPECT_EQ(1, guard->get().val);
        guard->get().val = 2;
        didExecute = true;
        EXPECT_EQ(2, guard->get().val);
      }

      {
        auto guard = guardedObj.getLockedGuard();
        if (didExecute) {
          EXPECT_EQ(guard->val, 2);
        } else {
          EXPECT_EQ(guard->val, 1);
        }
      }
    }
  }
}

TYPED_TEST_P(GuardedTest, test_try_fails_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  {
    auto guard = guardedObj.getLockedGuard();
    bool threadStarted = false;
    bool threadFinished = false;
    auto thr = std::thread([&] {
      threadStarted = true;
      bool didExecute = false;
      auto const res = guardedObj.tryUnderLock([&didExecute](UnderGuard& obj) {
        EXPECT_EQ(1, obj.val);
        obj.val = 2;
        didExecute = true;
        EXPECT_EQ(2, obj.val);
      });
      static_assert(std::is_same_v<std::optional<std::monostate> const, decltype(res)>);
      EXPECT_FALSE(res.has_value());
      EXPECT_FALSE(didExecute);
      threadFinished = true;
    });
    thr.join();
    EXPECT_TRUE(threadStarted);
    EXPECT_TRUE(threadFinished);
    EXPECT_EQ(guard->val, 1);
  }
}

TYPED_TEST_P(GuardedTest, test_try_guard_fails_access) {
  auto guardedObj = typename TestFixture::template Guarded<UnderGuard>{1};
  {
    auto guard = guardedObj.getLockedGuard();
    bool threadStarted = false;
    bool threadFinished = false;
    auto thr = std::thread([&] {
      threadStarted = true;
      bool didExecute = false;
      if (auto guard = guardedObj.tryLockedGuard()) {
        EXPECT_EQ(1, guard->get().val);
        guard->get().val = 2;
        didExecute = true;
        EXPECT_EQ(2, guard->get().val);
      }
      EXPECT_FALSE(didExecute);
      threadFinished = true;
    });
    thr.join();
    EXPECT_TRUE(threadStarted);
    EXPECT_TRUE(threadFinished);
    EXPECT_EQ(guard->val, 1);
  }
}

REGISTER_TYPED_TEST_CASE_P(GuardedTest, test_copy_allows_access, test_copy_waits_for_access,
                           test_assign_allows_access, test_assign_waits_for_access,
                           test_guard_allows_access, test_guard_waits_for_access,
                           test_guard_unlock_releases_mutex, test_do_allows_access,
                           test_do_waits_for_access, test_try_allows_access,
                           test_try_guard_allows_access, test_try_fails_access,
                           test_try_guard_fails_access, test_guard_unlock_releases_value);

template <template <typename> typename T>
struct ParamT {
  template <typename U>
  using instantiate = T<U>;
};

using TestedTypes =
    ::testing::Types<std::pair<std::mutex, ParamT<std::unique_lock>>,
                     std::pair<arangodb::basics::UnshackledMutex, ParamT<std::unique_lock>>,
                     std::pair<arangodb::Mutex, ParamT<std::unique_lock>>>;

INSTANTIATE_TYPED_TEST_CASE_P(GuardedTestInstantiation, GuardedTest, TestedTypes);

}  // namespace arangodb::tests
