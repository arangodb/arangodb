////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/FutureSharedLock.h"

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/lockfree/queue.hpp>

namespace {

using namespace std::chrono_literals;

struct MockScheduler;
using FutureSharedLock = arangodb::futures::FutureSharedLock<MockScheduler>;

struct MockScheduler {
  template<class Fn>
  void queue(Fn func) {
    funcs.push_back({std::move(func)});
  }

  using WorkHandle = int;

  template<class Fn>
  WorkHandle queueDelayed(Fn func, std::chrono::milliseconds delay) {
    delayedFuncs.emplace_back(std::move(func), delay);
    return 0;
  }

  void executeScheduled() {
    // the called functions might queue new stuff, so we need to move out
    // everything and clear the queue beforehand.
    decltype(funcs) ff = std::move(funcs);
    funcs.clear();
    for (auto& fn : ff) {
      fn();
    }
  }

  void executeNextDelayed() {
    ASSERT_FALSE(delayedFuncs.empty());
    auto f = std::move(delayedFuncs.front());
    delayedFuncs.pop_front();
    f.first(false);
  }
  std::vector<fu2::unique_function<void()>> funcs;
  std::deque<std::pair<std::function<void(bool)>, std::chrono::milliseconds>>
      delayedFuncs;
};

struct FutureSharedLockTest : public ::testing::Test {
  FutureSharedLockTest() : lock(scheduler) {}
  void TearDown() override {
    ASSERT_EQ(0, scheduler.funcs.size());
    ASSERT_EQ(0, scheduler.delayedFuncs.size());
  }

  MockScheduler scheduler;
  FutureSharedLock lock;
};

TEST_F(FutureSharedLockTest,
       asyncLockExclusive_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncLockExclusive().thenFinal([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncLockExclusive().thenFinal([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(FutureSharedLockTest,
       asyncLockExclusive_should_return_unresolved_future_when_locked) {
  lock.asyncLockExclusive().thenFinal([&](auto) {
    // try to lock again while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockExclusive();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(FutureSharedLockTest,
       unlock_should_post_the_next_owner_on_the_scheduler) {
  int called = 0;
  lock.asyncLockExclusive().thenFinal([&](auto) {
    ++called;
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    // we still hold the lock, so nothing must be queued on the scheduler yet
    EXPECT_EQ(0, scheduler.funcs.size());
  });
  EXPECT_EQ(1, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(2, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(3, called);
}

TEST_F(
    FutureSharedLockTest,
    asyncLockExclusive_should_return_unresolved_future_when_predecessor_has_shared_lock) {
  lock.asyncLockShared().thenFinal([&](auto) {
    // try to acquire exclusive lock while we hold the shared lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockExclusive();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(FutureSharedLockTest,
       asyncLockShared_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncLockShared().thenFinal([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncLockShared().thenFinal([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(
    FutureSharedLockTest,
    asyncLockShared_should_return_resolved_future_when_predecessor_has_shared_lock_and_is_active_or_finished) {
  lock.asyncLockShared().thenFinal([&](auto) {
    // try to lock again while we hold the shared lock
    // since we use shared access, this must succeed and return a resolved
    // future
    {
      auto fut = lock.asyncLockShared();
      EXPECT_TRUE(fut.isReady());
      fut = lock.asyncLockShared();
      EXPECT_TRUE(fut.isReady());
    }
    // the previous two futures are already finished. This implies that they
    // have been active, so this must also succeed and return a resolved future
    auto fut = lock.asyncLockShared();
    EXPECT_TRUE(fut.isReady());
  });
}

TEST_F(
    FutureSharedLockTest,
    asyncLockShared_should_return_unresolved_future_when_predecessor_has_exclusive_lock) {
  lock.asyncLockExclusive().thenFinal([&](auto) {
    // try to acquire shared lock while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockShared();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(
    FutureSharedLockTest,
    asyncLockShared_should_return_unresolved_future_when_predecessor_is_blocked) {
  lock.asyncLockExclusive().thenFinal([&](auto) {
    // try to acquire shared lock while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockShared();
    EXPECT_FALSE(fut.isReady());

    // try to acquire yet another shared lock
    // this will be queued after the previous one, and since that one is blocked
    // we must be blocked as well
    fut = lock.asyncLockShared();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(FutureSharedLockTest,
       unlock_shared_should_post_the_next_exclusive_owner_on_the_scheduler) {
  int called = 0;
  lock.asyncLockShared().thenFinal([&](auto) {
    ++called;
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    // we still hold the lock, so nothing must be queued on the scheduler yet
    EXPECT_EQ(0, scheduler.funcs.size());
  });
  EXPECT_EQ(1, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(2, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(3, called);
}

TEST_F(FutureSharedLockTest,
       unlock_exclusive_should_post_all_next_shared_requests_on_the_scheduler) {
  int called = 0;
  lock.asyncLockExclusive().thenFinal([&](auto) {
    ++called;
    lock.asyncLockShared().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockShared().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    // we still hold the lock, so nothing must be queued on the scheduler yet
    EXPECT_EQ(0, scheduler.funcs.size());
  });
  EXPECT_EQ(1, called);
  EXPECT_EQ(2, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(3, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(4, called);
}

TEST_F(FutureSharedLockTest,
       unlock_shared_should_post_next_exclusive_on_the_scheduler) {
  int called = 0;

  lock.asyncLockShared().thenFinal([&](auto) {
    ++called;
    lock.asyncLockShared().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockShared().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    EXPECT_EQ(3, called);
    // we still hold the lock, so nothing must be queued on the scheduler yet
    EXPECT_EQ(0, scheduler.funcs.size());
  });
  EXPECT_EQ(3, called);
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(4, called);
}

TEST_F(FutureSharedLockTest,
       unlock_shared_should_hand_over_ownership_to_next_active_shared) {
  int called = 0;
  FutureSharedLock::LockGuard lockGuard;
  lock.asyncLockShared().thenFinal([&](auto) {
    ++called;
    std::ignore = lock.asyncLockShared().thenValue([&](auto guard) {  //
      ++called;
      lockGuard = std::move(guard);
    });
    lock.asyncLockShared().thenFinal([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().thenFinal([&](auto) {  //
      ++called;
    });
    EXPECT_EQ(3, called);
    // we still hold the lock, so nothing must be queued on the scheduler yet
    EXPECT_EQ(0, scheduler.funcs.size());
  });

  // the first shared lock has been released, but the second one is still active
  // -> we still only have 3 calls and nothing queued
  EXPECT_EQ(3, called);
  EXPECT_EQ(0, scheduler.funcs.size());

  lockGuard.unlock();
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(4, called);
}

TEST_F(FutureSharedLockTest,
       asyncTryLockExclusiveFor_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncTryLockExclusiveFor(10ms).thenFinal([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncTryLockExclusiveFor(10ms).thenFinal([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(FutureSharedLockTest,
       asyncTryLockExclusiveFor_should_return_unresolved_future_when_locked) {
  lock.asyncTryLockExclusiveFor(10ms).thenFinal([&](auto) {
    // try to lock again while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncTryLockExclusiveFor(10ms);
    EXPECT_FALSE(fut.isReady());
    std::move(fut).thenFinal(
        [](auto result) { EXPECT_TRUE(result.hasValue()); });
  });
  scheduler.executeScheduled();  // cleanup

  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // cleanup
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockExclusiveFor_should_return_unresolved_future_when_predecessor_has_shared_lock) {
  lock.asyncLockShared().thenFinal([&](auto) {
    // try to acquire exclusive lock while we hold the shared lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncTryLockExclusiveFor(10ms);
    EXPECT_FALSE(fut.isReady());
    std::move(fut).thenFinal(
        [](auto result) { EXPECT_TRUE(result.hasValue()); });
  });
  scheduler.executeScheduled();  // cleanup

  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // cleanup
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockExclusiveFor_should_resolve_with_exception_when_timeout_is_reached) {
  FutureSharedLock::LockGuard lockGuard;
  bool resolvedWithTimeout = false;
  std::ignore = lock.asyncLockExclusive().thenValue([&](auto guard) {
    lockGuard = std::move(guard);
    lock.asyncTryLockExclusiveFor(10ms).thenFinal([&](auto result) {
      EXPECT_TRUE(result.hasException());
      try {
        result.throwIfFailed();
      } catch (::arangodb::basics::Exception const& e) {
        EXPECT_EQ(e.code(), TRI_ERROR_LOCK_TIMEOUT);
        resolvedWithTimeout = true;
      } catch (...) {
      }
    });
  });
  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // simulate timeout
  lockGuard.unlock();
  scheduler.executeScheduled();
  EXPECT_TRUE(resolvedWithTimeout);
}

TEST_F(FutureSharedLockTest,
       asyncTryLockSharedFor_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncTryLockSharedFor(10ms).thenFinal([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncTryLockSharedFor(10ms).thenFinal([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockSharedFor_should_return_resolved_future_when_predecessor_has_shared_lock_and_is_active_or_finished) {
  lock.asyncTryLockSharedFor(10ms).thenFinal([&](auto) {
    // try to lock again while we hold the shared lock
    // since we use shared access, this must succeed and return a resolved
    // future
    {
      auto fut = lock.asyncTryLockSharedFor(10ms);
      EXPECT_TRUE(fut.isReady());
      fut = lock.asyncTryLockSharedFor(10ms);
      EXPECT_TRUE(fut.isReady());
    }
    // the previous two futures are already finished. This implies that they
    // have been active, so this must also succeed and return a resolved future
    auto fut = lock.asyncTryLockSharedFor(10ms);
    EXPECT_TRUE(fut.isReady());
  });
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockSharedFor_should_return_unresolved_future_when_predecessor_has_exclusive_lock) {
  lock.asyncLockExclusive().thenFinal([&](auto) {
    // try to acquire shared lock while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncTryLockSharedFor(10ms);
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // cleanup
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockSharedFor_should_return_unresolved_future_when_predecessor_is_blocked) {
  lock.asyncLockExclusive().thenFinal([&](auto) {
    // try to acquire shared lock while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockShared();
    EXPECT_FALSE(fut.isReady());

    fut = lock.asyncTryLockSharedFor(10ms);
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // cleanup
}

TEST_F(
    FutureSharedLockTest,
    asyncTryLockSharedFor_should_resolve_with_exception_when_timeout_is_reached) {
  FutureSharedLock::LockGuard lockGuard;
  bool resolvedWithTimeout = false;
  std::ignore = lock.asyncLockExclusive().thenValue([&](auto guard) {
    lockGuard = std::move(guard);
    lock.asyncTryLockSharedFor(10ms).thenFinal([&](auto result) {
      EXPECT_TRUE(result.hasException());
      try {
        result.throwIfFailed();
      } catch (::arangodb::basics::Exception const& e) {
        EXPECT_EQ(e.code(), TRI_ERROR_LOCK_TIMEOUT);
        resolvedWithTimeout = true;
      } catch (...) {
      }
    });
  });
  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  scheduler.executeNextDelayed();  // simulate timeout
  lockGuard.unlock();
  scheduler.executeScheduled();
  EXPECT_TRUE(resolvedWithTimeout);
}

TEST_F(
    FutureSharedLockTest,
    unlock_should_skip_over_abandoned_node_when_scheduling_shared_lock_owners) {
  FutureSharedLock::LockGuard lockGuard;
  int called = 0;

  std::ignore = lock.asyncLockExclusive().thenValue([&](auto guard) {
    lockGuard = std::move(guard);
    // first acquire shared lock without timeout
    // -> this will become the new leader
    lock.asyncLockShared().thenFinal([&](auto) { ++called; });

    lock.asyncLockShared().thenFinal([&](auto) { ++called; });
    std::ignore = lock.asyncTryLockSharedFor(10ms).thenValue([&](auto) {
      FAIL();
      ++called;
    });
    lock.asyncLockShared().thenFinal([&](auto) { ++called; });
    std::ignore = lock.asyncTryLockSharedFor(10ms).thenValue([&](auto) {
      FAIL();
      ++called;
    });
  });
  ASSERT_EQ(2, scheduler.delayedFuncs.size());
  // simulate timeout
  scheduler.executeNextDelayed();
  scheduler.executeNextDelayed();

  lockGuard.unlock();
  scheduler.executeScheduled();
  EXPECT_EQ(3, called);
}

TEST_F(FutureSharedLockTest,
       lock_can_be_deleted_before_timeout_callback_is_executed) {
  {
    FutureSharedLock lock(scheduler);
    std::ignore = lock.asyncLockExclusive().thenValue(
        [&](auto guard) { std::ignore = lock.asyncTryLockSharedFor(10ms); });
    scheduler.executeScheduled();
  }
  ASSERT_EQ(1, scheduler.delayedFuncs.size());
  // simulate timeout - this should do nothing since the lock has been deleted
  scheduler.executeNextDelayed();
}

struct StressScheduler {
  struct Func {
    fu2::unique_function<void()> func;
  };

  StressScheduler() : scheduled(64) {}

  template<class Fn>
  void queue(Fn&& func) {
    scheduled.push(new Func{std::forward<Fn>(func)});
  }
  using WorkHandle = int;
  template<class Fn>
  WorkHandle queueDelayed(Fn&& func, std::chrono::milliseconds delay) {
    auto when = std::chrono::steady_clock::now() + delay;
    std::lock_guard lock(mutex);
    delayedFuncs.emplace(when, std::forward<Fn>(func));
    return 0;
  }

  void executeScheduled() {
    executeDelayed();

    Func* fn = nullptr;
    while (scheduled.pop(fn)) {
      fn->func();
      delete fn;
    }
  }

  void executeDelayed() {
    // it is enough for one thread to process the delayedFuncs
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      return;
    }

    // we move the functions out so we can release the lock before executing
    std::vector<std::function<void()>> funcs;
    auto now = std::chrono::steady_clock::now();
    auto it = delayedFuncs.begin();
    while (it != delayedFuncs.end()) {
      if (it->first > now) {
        break;
      }
      funcs.emplace_back([func = std::move(it->second)]() { func(false); });
      it = delayedFuncs.erase(it);
    }
    lock.unlock();

    for (auto& f : funcs) {
      f();
    }
  }

  boost::lockfree::queue<Func*> scheduled;

  std::mutex mutex;
  std::multimap<std::chrono::steady_clock::time_point,
                std::function<void(bool)>>
      delayedFuncs;
};

TEST(FutureSharedLockStressTest, parallel) {
  using FutureSharedLock = arangodb::futures::FutureSharedLock<StressScheduler>;
  StressScheduler scheduler;
  FutureSharedLock lock(scheduler);

  std::unordered_map<int, int> sharedData;

  constexpr int numThreads = 16;
  constexpr int numOpsPerThread = 4000000;

  std::vector<std::thread> threads;
  std::atomic<size_t> totalFound{0};
  std::atomic<size_t> lockTimeouts{0};
  std::atomic<unsigned> numTasks{0};
  threads.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i) {
    threads.push_back(std::thread([&scheduler, &lock, &sharedData, &totalFound,
                                   &numTasks, &lockTimeouts, id = i]() {
      std::mt19937 rnd(id);
      for (int j = 0; j < numOpsPerThread; ++j) {
        auto val = rnd();

        if ((val & 3) > 0 || numTasks > numThreads * 10) {
          scheduler.executeScheduled();
          continue;
        }
        numTasks++;
        val >>= 2;

        auto exclusiveFunc = [id, &sharedData, rnd,
                              &numTasks](auto val) mutable {
          --numTasks;
          val = (val >> 1) & 63;
          for (decltype(val) k = 0; k < val; ++k) {
            sharedData[rnd() & 1023] = id;
          }
        };

        auto sharedFunc = [id, &sharedData, rnd, &totalFound,
                           &numTasks](auto val) mutable {
          --numTasks;
          val = (val >> 1) & 63;
          for (decltype(val) k = 0; k < val; ++k) {
            auto it = sharedData.find(rnd() & 1023);
            if (it != sharedData.end() && it->second == id) {
              ++totalFound;
            }
          }
        };

        // perform some random write/read operations
        if (val & 1) {
          val = val >> 1;
          if (val & 1) {
            lock.asyncLockExclusive().thenFinal(
                [exclusiveFunc, val](auto) mutable { exclusiveFunc(val); });
          } else {
            auto timeout = val & 15;
            lock.asyncTryLockExclusiveFor(std::chrono::milliseconds(timeout))
                .thenFinal(
                    [exclusiveFunc, &lockTimeouts, val](auto res) mutable {
                      if (res.hasValue()) {
                        exclusiveFunc(val);
                      } else {
                        ++lockTimeouts;
                      }
                    });
          }
        } else {
          val = val >> 1;
          if (val & 1) {
            std::ignore = lock.asyncLockShared().thenValue(
                [sharedFunc, val](auto) mutable { sharedFunc(val); });
          } else {
            auto timeout = val & 15;
            lock.asyncTryLockSharedFor(std::chrono::milliseconds(timeout))
                .thenFinal([sharedFunc, &lockTimeouts, val](auto res) mutable {
                  if (res.hasValue()) {
                    sharedFunc(val);
                  } else {
                    ++lockTimeouts;
                  }
                });
          }
        }
      }

      scheduler.executeScheduled();
    }));
  }

  for (auto& t : threads) {
    t.join();
  }
  EXPECT_EQ(0, numTasks.load() - lockTimeouts.load());
  std::cout << "Found total " << totalFound.load() <<  //
      "\nLock timeouts " << lockTimeouts.load() << std::endl;
}

}  // namespace
