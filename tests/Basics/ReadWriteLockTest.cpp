////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/ReadWriteLock.h"
#include "Basics/ScopeGuard.h"
#include "Basics/system-compiler.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

using namespace arangodb::basics;

namespace {
struct Synchronizer {
  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;
  int waiting{0};

  void waitForStart() {
    std::unique_lock lock(mutex);
    ++waiting;
    cv.wait(lock, [&] { return ready; });
  }
  void start(int nr) {
    while (true) {
      {
        std::unique_lock lock(mutex);
        if (waiting >= nr) {
          ready = true;
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cv.notify_all();
  }
};

}  // namespace

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

  Synchronizer s;

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      s.waitForStart();

      for (size_t i = 0; i < iterations; ++i) {
        lock.lockWrite();
        ++counter;
        lock.unlock();
      }
    });
  }

  s.start(n);

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

  Synchronizer s;

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      s.waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWrite()) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  s.start(n);

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

  Synchronizer s;

  auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(60.0 /*seconds*/));
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      s.waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWriteFor(timeout)) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  s.start(n);

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

  Synchronizer s;

  auto timeout = std::chrono::microseconds(1);
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      s.waitForStart();
      for (size_t i = 0; i < iterations; ++i) {
        while (!lock.tryLockWriteFor(timeout)) {
        }
        ++counter;
        lock.unlock();
      }
    });
  }

  s.start(n);

  guard.fire();
  ASSERT_EQ(iterations * n, counter);
}

TEST(ReadWriteLockTest, testTryLockWriteForWakeUpReaders) {
  ReadWriteLock lock;

  std::vector<std::thread> threads;
  threads.reserve(3);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  Synchronizer s;

  // The main thread will hold the read lock for the duration of the test
  auto gotLock = lock.tryLockRead();
  ASSERT_TRUE(gotLock) << "Failed to get the read lock without concurrency";
  bool writeLockThreadCompleted = false;
  bool readLockThreadCompleted = false;

  // First thread tries to get the writeLock with a timeout
  threads.emplace_back(
      [&]() {
        s.waitForStart();
        auto timeout = std::chrono::milliseconds(100);
        auto gotLock = lock.tryLockWriteFor(timeout);
        EXPECT_FALSE(gotLock)
            << "We got a write lock although the read lock was held";
        writeLockThreadCompleted = true;
      });

  // Second thread tries to get the readLock, while the first thread is waiting
  // for the writeLock
  threads.emplace_back([&]() {
    s.waitForStart();
    // This is still a race with the write locker
    // It may happen that we try to lock read before write => we pass here.
    // If we cannot get the readlock in an instant, we know the write locker is
    // in queue.
    size_t retryCounter = 100;
    while (lock.tryLockRead()) {
      lock.unlock();
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      retryCounter--;
      if (retryCounter == 0) {
        ASSERT_FALSE(true) << "A queued write lock did not block the reader "
                              "from getting the lock";
        return;
      }
    }
    // NOTE: This time out is **much larger** than the write timeout.
    // So we need to be woken up if the Writer is released. If not
    // (old buggy behaviour), we will still wait for 30 seconds:
    auto timeout = std::chrono::seconds(30);
    auto gotLock = lock.tryLockReadFor(timeout);
    EXPECT_TRUE(gotLock)
        << "We did not get the read lock after the write lock got into timeout";
    lock.unlock();
    readLockThreadCompleted = true;
  });
  s.start(2);

  guard.fire();
  EXPECT_TRUE(readLockThreadCompleted)
      << "Did not complete the read lock thread";
  EXPECT_TRUE(writeLockThreadCompleted)
      << "Did not complete the write lock thread";
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

  Synchronizer s;

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    if (i >= n / 2) {
      threads.emplace_back([&]() {
        s.waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          while (!lock.tryLockWrite()) {
          }
          ++counter;
          lock.unlock();
        }
      });
    } else {
      threads.emplace_back([&]() {
        s.waitForStart();
        [[maybe_unused]] uint64_t total = 0;
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockRead();
          total += counter;
          lock.unlock();
        }
      });
    }
  }

  s.start(n);

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

  Synchronizer s;

  auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(60.0 /*seconds*/));
  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  for (size_t i = 0; i < n; ++i) {
    if (i == 0 || i == 1) {
      threads.emplace_back([&]() {
        s.waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockWrite();
          ++counter;
          lock.unlock();
        }
      });
    } else if (i == 2 || i == 3) {
      threads.emplace_back([&]() {
        s.waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          while (!lock.tryLockWrite()) {
          }
          ++counter;
          lock.unlock();
        }
      });
    } else if (i == 4 || i == 5) {
      threads.emplace_back([&]() {
        s.waitForStart();
        for (size_t i = 0; i < iterations; ++i) {
          while (!lock.tryLockWriteFor(timeout)) {
          }
          ++counter;
          lock.unlock();
        }
      });
    } else {
      threads.emplace_back([&]() {
        s.waitForStart();
        [[maybe_unused]] uint64_t total = 0;
        for (size_t i = 0; i < iterations; ++i) {
          lock.lockRead();
          total += counter;
          lock.unlock();
        }
      });
    }
  }

  s.start(n);

  guard.fire();
  ASSERT_EQ(iterations * 6, counter);
}

TEST(ReadWriteLockTest, testRandomMixedParallel) {
  ReadWriteLock lock;

  constexpr size_t n = 6;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  Synchronizer s;

  constexpr size_t iterations = 5000000;
  uint64_t counter = 0;

  std::atomic<size_t> total = 0;

  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back([&]() {
      s.waitForStart();
      size_t expected = 0;
      for (size_t i = 0; i < iterations; ++i) {
        auto r = arangodb::RandomGenerator::interval(static_cast<uint32_t>(4));
        switch (r) {
          case 0: {
            lock.lockWrite();
            ++counter;
            ++expected;
            lock.unlock();
            break;
          }
          case 1: {
            if (lock.tryLockWrite()) {
              ++counter;
              ++expected;
              lock.unlock();
            }
            break;
          }
          case 2: {
            if (lock.tryLockRead()) {
              [[maybe_unused]] uint64_t value = counter;
              lock.unlock();
            }
            break;
          }
          case 3: {
            lock.lockRead();
            [[maybe_unused]] uint64_t value = counter;
            lock.unlock();
            break;
          }
          case 4: {
            if (lock.tryLockWriteFor(std::chrono::microseconds(
                    arangodb::RandomGenerator::interval(
                        static_cast<uint32_t>(1000))))) {
              ++counter;
              ++expected;
              lock.unlock();
            }
            break;
          }
          default: {
            ADB_UNREACHABLE;
          }
        }
      }

      total.fetch_add(expected);
    });
  }

  s.start(n);

  guard.fire();
  ASSERT_EQ(total, counter);
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

TEST(ReadWriteLockTest, readerOverflow) {
  // this is a regression test for the old version where we only used 16 bits
  // for the reader counter. Since we can have many more readers than threads,
  // this limit could easily be reached. Note that we have no similar test for a
  // writer overflow since we would actually need 2^15 threads to reach that
  // limit.
  ReadWriteLock lock;

  for (unsigned i = 0; i < (1 << 16); ++i) {
    ASSERT_TRUE(lock.tryLockRead()) << "tryLockRead failed at iteration " << i;
  }
  ASSERT_FALSE(lock.tryLockWrite())
      << "tryLockWrite succeeded even though we have active readers";
}

TEST(ReadWriteLockTest, stringifyLockState) {
  auto waitUntil = [](ReadWriteLock& lock, std::string_view expected) {
    int iterations = 0;
    std::string state;
    while (++iterations < 500) {
      state = lock.stringifyLockState();
      if (state.find(expected) != std::string::npos) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return state;
  };

  {
    ReadWriteLock lock;
    auto state = lock.stringifyLockState();
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s)");
  }

  {
    ReadWriteLock lock;
    lock.lockRead();
    auto state = lock.stringifyLockState();
    ASSERT_EQ(state, "1 active reader(s), 0 queued writer(s)");

    lock.lockRead();
    state = lock.stringifyLockState();
    ASSERT_EQ(state, "2 active reader(s), 0 queued writer(s)");

    lock.lockRead();
    state = lock.stringifyLockState();
    ASSERT_EQ(state, "3 active reader(s), 0 queued writer(s)");

    lock.unlockRead();

    state = lock.stringifyLockState();
    ASSERT_EQ(state, "2 active reader(s), 0 queued writer(s)");
  }

  {
    ReadWriteLock lock;
    lock.lockWrite();
    auto state = lock.stringifyLockState();
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s), write-locked");
  }

  {
    ReadWriteLock lock;
    lock.lockWrite();
    auto state = lock.stringifyLockState();
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s), write-locked");
  }

  {
    SCOPED_TRACE("reader blocks writer");

    ReadWriteLock lock;
    lock.lockRead();

    std::jthread background{[&lock]() {
      // this will block until we release the read-lock
      lock.lockWrite();
    }};

    auto state = waitUntil(lock, "1 queued writer(s)");
    ASSERT_EQ(state, "1 active reader(s), 1 queued writer(s)");
    lock.unlockRead();

    state = waitUntil(lock, "write-locked");
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s), write-locked");
  }

  {
    SCOPED_TRACE("writer blocks reader");

    ReadWriteLock lock;
    lock.lockWrite();

    std::jthread background{[&lock]() {
      // this will block until we release the write-lock
      lock.lockRead();
    }};

    auto state = lock.stringifyLockState();
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s), write-locked");
    lock.unlockWrite();

    state = waitUntil(lock, "1 active reader(s)");
    ASSERT_EQ(state, "1 active reader(s), 0 queued writer(s)");
  }

  {
    SCOPED_TRACE("writer blocks writer");

    ReadWriteLock lock;
    lock.lockWrite();

    std::jthread background{[&lock]() {
      // this will block until we release the write-lock
      lock.lockWrite();
    }};

    auto state = waitUntil(lock, "1 queued writer(s)");
    ASSERT_EQ(state, "0 active reader(s), 1 queued writer(s), write-locked");
    lock.unlockWrite();

    state = waitUntil(lock, "0 queued writer(s)");
    ASSERT_EQ(state, "0 active reader(s), 0 queued writer(s), write-locked");
  }
}
