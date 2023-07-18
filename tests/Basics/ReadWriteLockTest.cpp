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
#include "Basics/ReadWriteLock.h"
#include "Basics/ScopeGuard.h"

#include "gtest/gtest.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

using namespace arangodb::basics;

TEST(ReadWriteLockTest, testLockWriteParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      waitForStart();

      for (size_t i = 0; i < iterations; ++i) {
        lock.lockWrite();
        ++counter;
        lock.unlock();
      }
    });
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * n, counter);
}

TEST(ReadWriteLockTest, testTryLockWriteParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWrite()) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * n, counter);
}

TEST(ReadWriteLockTest, testTryLockWriteForParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(60.0 /*seconds*/));
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWriteFor(timeout)) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * n, counter);
}

TEST(ReadWriteLockTest, testTryLockWriteForParallelLowTimeout) {
  ReadWriteLock lock;

  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  auto timeout = std::chrono::microseconds(1);
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWriteFor(timeout)) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * n, counter);
}

TEST(ReadWriteLockTest, testLockWriteLockReadParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    if (i >= n / 2) {
      threads.emplace_back([&]() {
        waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockWrite();
          ++counter;
          lock.unlock();
        }
      });
    } else {
      threads.emplace_back([&]() {
        waitForStart();
        [[maybe_unused]] uint64_t total = 0;
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockRead();
          total += counter;
          lock.unlock();
        }
      });
    }
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * (n / 2), counter);
}

TEST(ReadWriteLockTest, testMixedParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 8;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  auto waitForStart = [&]() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  };
  auto start = [&]() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  };

  auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(60.0 /*seconds*/));
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    if (i == 0 || i == 1) {
      threads.emplace_back([&]() {
        waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockWrite();
          ++counter;
          lock.unlock();
        }
      });
    } else if (i == 2 || i == 3) {
      threads.emplace_back([&]() {
        waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          while (!lock.tryLockWrite()) {
          }
          ++counter;
          lock.unlock();
        }
      });
    } else if (i == 4 || i == 5) {
      threads.emplace_back([&]() {
        waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          while (!lock.tryLockWriteFor(timeout)) {
          }
          ++counter;
          lock.unlock();
        }
      });
    } else {
      threads.emplace_back([&]() {
        waitForStart();
        [[maybe_unused]] uint64_t total = 0;
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockRead();
          total += counter;
          lock.unlock();
        }
      });
    }
  }

  start();

  guard.fire();
  ASSERT_EQ(iterations * 6, counter);
}

TEST(ReadWriteLockTest, testTryLockWrite) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try lock write
  ASSERT_TRUE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again, with timeout
  ASSERT_FALSE(lock.tryLockWriteFor(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try read-locking
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(ReadWriteLockTest, testLockWrite) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // lock write
  lock.lockWrite();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again, with timeout
  ASSERT_FALSE(lock.tryLockWriteFor(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try read-locking
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(ReadWriteLockTest, testTryLockRead) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try lock read
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try read-locking again
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // read-lock again
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try write-locking
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try write-locking again, with timeout
  ASSERT_FALSE(lock.tryLockWriteFor(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // unlock one level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());

  // unlock one another level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());

  // unlock final level
  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_TRUE(lock.tryLockWrite());
}

TEST(ReadWriteLockTest, testLockRead) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // lock read
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try read-locking again
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // read-lock again
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try write-locking
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try write-locking again, with timeout
  ASSERT_FALSE(lock.tryLockWriteFor(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // unlock one level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());

  // unlock one another level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());

  // unlock final level
  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_TRUE(lock.tryLockWrite());
}

TEST(ReadWriteLockTest, testLockWriteAttempted) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // lock write
  ASSERT_TRUE(lock.tryLockWriteFor(std::chrono::microseconds(1000000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try locking again
  ASSERT_FALSE(lock.tryLockWriteFor(std::chrono::microseconds(1000000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}
