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

#include "Futures/FutureSharedLock.h"

#include "gtest/gtest.h"

#include <memory>
#include <random>
#include <boost/lockfree/queue.hpp>

namespace {

struct MockScheduler;
using FutureSharedLock = arangodb::futures::FutureSharedLock<MockScheduler>;

struct MockScheduler {
  template<class Fn>
  void queue(Fn func) {
    funcs.push_back(std::move(func));
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
  std::vector<std::function<void()>> funcs;
};

struct FuturesSharedLockTest : public ::testing::Test {
  FuturesSharedLockTest() : lock(scheduler) {}
  void TearDown() override { ASSERT_EQ(0, scheduler.funcs.size()); }

  MockScheduler scheduler;
  FutureSharedLock lock;
};

TEST_F(FuturesSharedLockTest,
       asyncLockExclusive_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncLockExclusive().then([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncLockExclusive().then([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(FuturesSharedLockTest,
       asyncLockExclusive_should_return_unresolved_future_when_locked) {
  lock.asyncLockExclusive().then([&](auto) {
    // try to lock again while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockExclusive();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(FuturesSharedLockTest,
       unlock_should_post_the_next_owner_on_the_scheduler) {
  int called = 0;
  lock.asyncLockExclusive().then([&](auto) {
    ++called;
    lock.asyncLockExclusive().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().then([&](auto) {  //
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

TEST_F(FuturesSharedLockTest,
       asyncLockShared_should_return_resolved_future_when_unlocked) {
  int called = 0;
  lock.asyncLockShared().then([&](auto) { ++called; });
  EXPECT_EQ(1, called);

  lock.asyncLockShared().then([&](auto) { ++called; });
  EXPECT_EQ(2, called);
}

TEST_F(
    FuturesSharedLockTest,
    asyncLockShared_should_return_resolved_future_when_predecessor_has_shared_lock_and_is_active_or_finished) {
  lock.asyncLockShared().then([&](auto) {
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
    FuturesSharedLockTest,
    asyncLockExclusive_should_return_unresolved_future_when_predecessor_has_shared_lock) {
  lock.asyncLockShared().then([&](auto) {
    // try to acquire exclusive lock while we hold the shared lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockExclusive();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(
    FuturesSharedLockTest,
    asyncLockShared_should_return_unresolved_future_when_predecessor_has_exclusive_lock) {
  lock.asyncLockExclusive().then([&](auto) {
    // try to acquire shared lock while we hold the exclusive lock
    // this must return a future that is not yet resolved
    auto fut = lock.asyncLockShared();
    EXPECT_FALSE(fut.isReady());
  });
  scheduler.executeScheduled();  // cleanup
}

TEST_F(FuturesSharedLockTest,
       unlock_shared_should_post_the_next_exclusive_owner_on_the_scheduler) {
  int called = 0;
  lock.asyncLockShared().then([&](auto) {
    ++called;
    lock.asyncLockExclusive().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().then([&](auto) {  //
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

TEST_F(FuturesSharedLockTest,
       unlock_exclusive_should_post_all_next_shared_requests_on_the_scheduler) {
  int called = 0;
  lock.asyncLockExclusive().then([&](auto) {
    ++called;
    lock.asyncLockShared().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockShared().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().then([&](auto) {  //
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

TEST_F(FuturesSharedLockTest,
       unlock_shared_should_post_next_exclusive_on_the_scheduler) {
  int called = 0;

  lock.asyncLockShared().then([&](auto) {
    ++called;
    lock.asyncLockShared().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockShared().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().then([&](auto) {  //
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

TEST_F(FuturesSharedLockTest,
       unlock_shared_should_hand_over_ownership_to_next_active_shared) {
  int called = 0;
  arangodb::futures::SharedLockGuard lockGuard;
  lock.asyncLockShared().then([&](auto) {
    ++called;
    lock.asyncLockShared().thenValue([&](auto guard) {  //
      ++called;
      lockGuard = std::move(guard);
    });
    lock.asyncLockShared().then([&](auto) {  //
      ++called;
    });
    lock.asyncLockExclusive().then([&](auto) {  //
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

TEST_F(FuturesSharedLockTest, simulate_blocked_shared_follower) {
  arangodb::futures::SharedLockGuard lockGuard;
  lock.asyncLockShared().thenValue([&](auto guard) {  //
    lockGuard = std::move(guard);
  });

  auto* tail = lock.tail();
  EXPECT_EQ(FutureSharedLock::State::kSharedActiveLeader, tail->state.load());
  EXPECT_EQ(nullptr, tail->next.load());

  lock.asyncLockShared().then([](auto) {});

  auto* next = tail->next.load();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ(FutureSharedLock::State::kSharedFinished, next->state.load());

  // simulate that our next request has observer its predecessor as blocked and
  // therefore
  int called = 0;
  next->promise = decltype(next->promise){};
  next->promise.getFuture().then([&](auto) { ++called; });
  next->state.store(FutureSharedLock::State::kSharedBlocked);

  lockGuard.unlock();
  EXPECT_EQ(1, scheduler.funcs.size());
  scheduler.executeScheduled();
  EXPECT_EQ(1, called);
}

struct StressScheduler {
  struct Func {
    std::function<void()> func;
  };

  StressScheduler() : scheduled(64) {}

  template<class Fn>
  void queue(Fn&& func) {
    scheduled.push(new Func{std::forward<Fn>(func)});
  }

  void executeScheduled() {
    Func* fn = nullptr;
    while (scheduled.pop(fn)) {
      fn->func();
      delete fn;
    }
  }

  boost::lockfree::queue<Func*> scheduled;
};

TEST(FuturesSharedLockStressTest, parallel) {
  using FutureSharedLock = arangodb::futures::FutureSharedLock<StressScheduler>;
  StressScheduler scheduler;
  FutureSharedLock lock(scheduler);

  std::unordered_map<int, int> sharedData;

  constexpr int numThreads = 1;
  constexpr int numOpsPerThread = 400000;

  std::vector<std::thread> threads;
  std::atomic<size_t> totalFound{0};
  threads.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i) {
    threads.push_back(
        std::thread([&scheduler, &lock, &sharedData, &totalFound, id = i]() {
          std::mt19937 rnd(id);
          size_t found = 0;
          for (int j = 0; j < numOpsPerThread; ++j) {
            auto val = rnd();
            if ((val & 3) > 0) {
              scheduler.executeScheduled();
            }
            val >>= 2;

            // perform some random write/read operations
            if (val & 1) {
              lock.asyncLockExclusive().then([&](auto) {
                val = (val >> 1) & 63;
                for (decltype(val) k = 0; k < val; ++k) {
                  sharedData[rnd() & 1023] = id;
                }
              });
            } else {
              lock.asyncLockShared().then([&](auto) {
                val = (val >> 1) & 63;
                for (decltype(val) k = 0; k < val; ++k) {
                  auto it = sharedData.find(rnd() & 1023);
                  if (it != sharedData.end() && it->second == id) {
                    ++found;
                  }
                }
              });
            }
          }

          scheduler.executeScheduled();
          totalFound += found;
        }));
  }

  for (auto& t : threads) {
    t.join();
  }
  std::cout << "Found total " << totalFound.load() << std::endl;
}

}  // namespace