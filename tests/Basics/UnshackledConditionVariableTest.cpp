////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <Basics/UnshackledConditionVariable.h>

#include "ThreadTestHelper.h"

#include <fmt/core.h>

#include <array>
#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;

/// Basic test of cv functionality.
TEST(UnshackledConditionVariableTest, basic_test) {
  auto mutex = UnshackledMutex();
  auto cv = UnshackledConditionVariable();
  bool ready = false;
  bool workerDone = false;

  auto worker = [&] {
    {
      std::unique_lock lock(mutex);
      cv.wait(lock, [&] { return ready; });

      workerDone = true;
    }
    cv.notify_one();
  };

  auto workerThread = std::thread(worker);

  {
    std::unique_lock lock(mutex);
    ready = true;
  }
  cv.notify_one();

  {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return workerDone; });
  }

  workerThread.join();
}

/// This test starts two threads, which share an integer `step`, starting with
/// 0. Both threads count a separate integer `i` from 0 up to some maximum
/// value. One thread wait()s on even `i`s for `step` to increase, and on odd
/// `i`s increases `step` and calls `notify_one`(). The other thread does the
/// same, except with even and odd swapped.
/// This test would work the same with std::mutex and std::condition_variable.
TEST(UnshackledConditionVariableTest, sequent_wakeups) {
  using namespace std::chrono_literals;
  auto mutex = UnshackledMutex();
  auto cv = UnshackledConditionVariable();
  auto step = 0u;
  auto constexpr numSteps = 10000u;

  auto even = [](auto i) { return i % 2 == 0; };
  auto odd = [](auto i) { return i % 2 == 1; };
  auto go = std::atomic<bool>{false};
  auto evenThreadReady = std::atomic<bool>{false};
  auto oddThreadReady = std::atomic<bool>{false};
  auto evenThreadFinished = std::atomic<bool>{false};
  auto oddThreadFinished = std::atomic<bool>{false};

  auto evenWaiter = [&] {
    evenThreadReady.store(true, std::memory_order_release);
    while (!go.load(std::memory_order_acquire)) {
    }
    for (auto i = 0u; i < numSteps; ++i) {
      if (even(i)) {
        auto lock = std::unique_lock(mutex);
        cv.wait(lock, [&] { return step == i + 1; });
      } else {
        {
          auto lock = std::unique_lock(mutex);
          ++step;
        }
        cv.notify_one();
      }
    }
    evenThreadFinished.store(true, std::memory_order_release);
  };
  auto oddWaiter = [&] {
    oddThreadReady.store(true, std::memory_order_release);
    while (!go.load(std::memory_order_acquire)) {
    }
    for (auto i = 0u; i < numSteps; ++i) {
      if (odd(i)) {
        auto lock = std::unique_lock(mutex);
        cv.wait(lock, [&] { return step == i + 1; });
      } else {
        {
          auto lock = std::unique_lock(mutex);
          ++step;
        }
        cv.notify_one();
      }
    }
    oddThreadFinished.store(true, std::memory_order_release);
  };

  auto evenThread = std::thread(evenWaiter);
  auto oddThread = std::thread(oddWaiter);

  while (!evenThreadReady.load(std::memory_order_acquire) ||
         !oddThreadReady.load(std::memory_order_acquire)) {
  }
  go.store(true, std::memory_order_release);

  auto timeout = std::chrono::steady_clock::now() + 5s;
  while (!(evenThreadFinished.load(std::memory_order_acquire) &&
           oddThreadFinished.load(std::memory_order_acquire)) &&
         std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(200us);
  }
  auto const oddDone = oddThreadFinished.load(std::memory_order_acquire);
  auto const evenDone = evenThreadFinished.load(std::memory_order_acquire);
  EXPECT_TRUE(oddDone);
  EXPECT_TRUE(evenDone);
  // Note that the following assert failing means not joining the threads
  // and thus crashing. Just detaching them would be UB because of the local
  // variables they're accessing.
  ASSERT_TRUE(oddDone && evenDone);

  evenThread.join();
  oddThread.join();
}

/// This test starts 3 threads, each going to `numSteps` iterations.
/// In each step, one thread waits for `step` to increase, the second locks the
/// mutex and increases `step`, while the third one unlocks the mutex (after
/// waiting for the second to signal it's done).
/// This test would not work with std::mutex and std::condition_variable (or
/// at least be UB), because the mutex here is unlocked in another thread
/// than the one it was locked in.
TEST(UnshackledConditionVariableTest,
     sequent_rotating_waits_locks_and_unlocks) {
  using namespace std::chrono_literals;
  auto mutex = UnshackledMutex();
  auto cv = UnshackledConditionVariable();
  auto step = 0u;
  auto static constexpr numSteps = 10000u;
  auto static constexpr numRoles = 3;

  enum Role : unsigned {
    kWaiter = 0,
    kLocker = 1,
    kUnlocker = 2,
    kMax = kUnlocker,
  };
  static_assert(numRoles == Role::kMax + 1);

  auto const roleByRoleIdx = [](auto roleIdx) constexpr {
    return [=](auto i) { return Role((i + roleIdx) % 3); };
  };
  auto go = std::atomic<bool>{false};
  auto lockNext = std::atomic<unsigned>{0};
  auto threadsReady = std::array<std::atomic<bool>, numRoles>();
  auto threadsFinished = std::array<std::atomic<bool>, numRoles>();
  std::fill(threadsReady.begin(), threadsReady.end(), false);
  std::fill(threadsFinished.begin(), threadsFinished.end(), false);

  auto const threadByRoleIdx = [&](auto roleIdx) constexpr {
    return [role = roleByRoleIdx(roleIdx), &mutex, &cv, &step, &go, &lockNext,
            &ready = threadsReady[roleIdx],
            &finished = threadsFinished[roleIdx]]() {
      ready.store(true, std::memory_order_release);
      while (!go.load(std::memory_order_acquire)) {
      }
      for (auto i = 0u; i < numSteps; ++i) {
        switch (role(i)) {
          break;
          case kWaiter: {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] { return step == i + 1; });
          } break;
          case kLocker: {
            while (lockNext.load(std::memory_order_release) != i) {
            }
            mutex.lock();
            ++step;
            lockNext.store(i + 1, std::memory_order_acquire);
          } break;
          case kUnlocker: {
            while (lockNext.load(std::memory_order_release) != i + 1) {
            }
            mutex.unlock();
            cv.notify_one();
          }
        }
      }
      finished.store(true, std::memory_order_release);
    };
  };

  auto threads = std::array<std::thread, numRoles>{};
  std::generate(threads.begin(), threads.end(), [&, i = 0]() mutable {
    return std::thread(threadByRoleIdx(i++));
  });

  auto constexpr all_true = [](auto const& container) {
    return std::all_of(std::begin(container), std::end(container),
                       [](auto const& value) {
                         return value.load(std::memory_order_acquire);
                       });
  };

  while (!all_true(threadsReady)) {
  }

  go.store(true, std::memory_order_release);

  auto timeout = std::chrono::steady_clock::now() + 5s;
  while (!all_true(threadsFinished) &&
         std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(200us);
  }

  bool anyThreadNotDone = false;

  for (auto i = 0; i < numRoles; ++i) {
    auto const done = threadsFinished[i].load(std::memory_order_acquire);
    EXPECT_TRUE(done) << "Thread #" << i << " (of " << numRoles
                      << ") didn't finish in time";
    if (!done) {
      anyThreadNotDone = true;
    }
  }

  // Note that the following assert failing means not joining the threads
  // and thus crashing. Just detaching them would be UB because of the local
  // variables they're accessing.
  ASSERT_FALSE(anyThreadNotDone);

  for (auto& thread : threads) {
    thread.join();
  }
}
