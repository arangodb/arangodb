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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/RecursiveLocker.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace arangodb::basics;

TEST(RecursiveLockerTest, testRecursiveMutexNoAcquire) {
  arangodb::Mutex mutex;
  std::atomic<std::thread::id> owner;

  RECURSIVE_MUTEX_LOCKER_NAMED(locker, mutex, owner, false);
  ASSERT_FALSE(locker.isLocked());

  locker.lock();
  ASSERT_TRUE(locker.isLocked());

  locker.unlock();
  ASSERT_FALSE(locker.isLocked());
}

TEST(RecursiveLockerTest, testRecursiveMutexAcquire) {
  arangodb::Mutex mutex;
  std::atomic<std::thread::id> owner;

  RECURSIVE_MUTEX_LOCKER_NAMED(locker, mutex, owner, true);
  ASSERT_TRUE(locker.isLocked());
  
  locker.unlock();
  ASSERT_FALSE(locker.isLocked());
}

TEST(RecursiveLockerTest, testRecursiveMutexLockUnlock) {
  arangodb::Mutex mutex;
  std::atomic<std::thread::id> owner;

  RECURSIVE_MUTEX_LOCKER_NAMED(locker, mutex, owner, true);
  ASSERT_TRUE(locker.isLocked());
  
  for (int i = 0; i < 100; ++i) {
    locker.unlock();
    ASSERT_FALSE(locker.isLocked());
    locker.lock();
    ASSERT_TRUE(locker.isLocked());
  }
  
  ASSERT_TRUE(locker.isLocked());
  locker.unlock();
  ASSERT_FALSE(locker.isLocked());
}

TEST(RecursiveLockerTest, testRecursiveMutexNested) {
  arangodb::Mutex mutex;
  std::atomic<std::thread::id> owner;

  RECURSIVE_MUTEX_LOCKER_NAMED(locker1, mutex, owner, true);
  ASSERT_TRUE(locker1.isLocked());

  {
    RECURSIVE_MUTEX_LOCKER_NAMED(locker2, mutex, owner, true);
    ASSERT_TRUE(locker2.isLocked());
  
    {
      RECURSIVE_MUTEX_LOCKER_NAMED(locker3, mutex, owner, true);
      ASSERT_TRUE(locker3.isLocked());
    }

    ASSERT_TRUE(locker2.isLocked());
  }

  ASSERT_TRUE(locker1.isLocked());

  locker1.unlock();
  ASSERT_FALSE(locker1.isLocked());
}

TEST(RecursiveLockerTest, testRecursiveMutexMultiThreaded) {
  arangodb::Mutex mutex;
  std::atomic<std::thread::id> owner;

  // number of threads started
  std::atomic<int> started{0};

  // shared variables, only protected by mutexes
  uint64_t total = 0;
  uint64_t x = 0;

  constexpr int n = 4;
  constexpr int iterations = 100000;

  std::vector<std::thread> threads;

  for (int i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      ++started;
      while (started < n) { /*spin*/ }

      for (int i = 0; i < iterations; ++i) {
        RECURSIVE_MUTEX_LOCKER_NAMED(locker1, mutex, owner, true);
        ASSERT_TRUE(locker1.isLocked());

        total++;
        x = x + 1;
        
        {
          RECURSIVE_MUTEX_LOCKER_NAMED(locker2, mutex, owner, true);
          ASSERT_TRUE(locker2.isLocked());
        
          x = x + 1;
        }
      }
    });
  }

  for (int i = 0; i < n; ++i) {
    threads[i].join();
  }
  
  ASSERT_EQ(n * iterations, total);
  ASSERT_EQ(n * iterations * 2, x);
}
