////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include <Basics/UnshackledMutex.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @brief The constructor of WorkerThread starts a thread, which immediately
 * starts waiting on a condition variable. The execute() method takes a
 * callback, which is passed to the waiting thread and is then executed by it,
 * while execute() returns.
 */
struct WorkerThread : std::enable_shared_from_this<WorkerThread> {
  WorkerThread() = default;

  void run() {
    // This can't be done in the constructor (shared_from_this)
    _thread = std::thread([self = this->shared_from_this()] {
      auto guard = std::unique_lock(self->_mutex);

      auto wait = [&] {
        self->_cv.wait(guard, [&] {
          return self->_callback != nullptr || self->_stopped;
        });
      };

      for (wait(); !self->_stopped; wait()) {
        self->_callback.operator()();
        self->_callback = {};
      }
    });
  }

  void execute(std::function<void()> callback) {
    ASSERT_TRUE(_thread.joinable());
    ASSERT_TRUE(callback);
    {
      auto guard = std::unique_lock(_mutex);
      ASSERT_TRUE(!_stopped);
      ASSERT_FALSE(_callback);
      std::swap(_callback, callback);
    }

    _cv.notify_one();
  }

  void stop() {
    {
      auto guard = std::unique_lock(_mutex);
      _stopped = true;
    }

    _cv.notify_one();
  }

 private:
  std::thread _thread;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::function<void()> _callback;
  bool _stopped = false;
};

auto operator<<(WorkerThread& workerThread, std::function<void()> callback) {
  return workerThread.execute(std::move(callback));
}

// Used as memoizable thread indexes
enum Thread {
  ALPHA = 0,
  BETA = 1,
  GAMMA = 2,
  DELTA = 3,
  EPSILON = 4,
  ZETA = 5,
  ETA = 6,
  THETA = 7,
  IOTA = 8,
  KAPPA = 9,
  LAMBDA = 10,
  MU = 11,
  NU = 12,
  XI = 13,
  OMIKRON = 14,
  PI = 15,
  RHO = 16,
  SIGMA = 17,
  TAU = 18,
  UPSILON = 19,
  PHI = 20,
  CHI = 21,
  PSI = 22,
  OMEGA = 23,
};

// Note that this test will probably succeed even for std::mutex or others,
// unless you run it with TSan.
TEST(UnshackledMutexTest, interleavedThreadsTest) {
  using namespace std::chrono_literals;

  constexpr auto countUpTo = [](Thread thread) -> std::size_t {
    return static_cast<std::underlying_type_t<Thread>>(thread) + 1;
  };

  constexpr auto numThreads = countUpTo(Thread::EPSILON);
  static_assert(numThreads == 5);

  auto threads = std::array<std::shared_ptr<WorkerThread>, numThreads>();
  std::generate(threads.begin(), threads.end(),
                [] { return std::make_shared<WorkerThread>(); });
  for (auto& it : threads) {
    it->run();
  }

  auto waitUntilAtMost = [](auto predicate, auto sleepTime, auto timeout) {
    for (auto start = std::chrono::steady_clock::now(); !predicate();
         std::this_thread::sleep_for(sleepTime)) {
      ASSERT_LT(std::chrono::steady_clock::now(), start + timeout);
    }
  };

  auto testee = ::arangodb::basics::UnshackledMutex();

  // run a code block in the named thread
#define RUN(thr) *threads[thr] << [&]

  constexpr auto numCheckpoints = numThreads;
  auto checkpointReached = std::array<std::atomic<bool>, numCheckpoints>();
  std::fill(checkpointReached.begin(), checkpointReached.end(), false);

  // ALPHA takes the lock at first
  RUN(ALPHA) {
    testee.lock();
    checkpointReached[ALPHA] = true;
  };

  waitUntilAtMost([&]() -> bool { return checkpointReached[ALPHA]; }, 1us, 1s);

  // BETA has to wait for the lock as ALPHA still holds it
  RUN(BETA) {
    testee.lock();
    checkpointReached[BETA] = true;
  };

  std::this_thread::sleep_for(1ms);
  ASSERT_FALSE(checkpointReached[BETA]);

  // ALPHA releases the lock it took, allowing BETA to take it and continue
  RUN(ALPHA) { testee.unlock(); };

  // BETA should now finish its last callback
  waitUntilAtMost([&]() -> bool { return checkpointReached[BETA]; }, 1us, 1s);

  // BETA holds the lock now
  ASSERT_FALSE(testee.try_lock());

  // GAMMA has to wait for the lock as BETA still holds it
  RUN(GAMMA) {
    testee.lock();
    checkpointReached[GAMMA] = true;
  };

  std::this_thread::sleep_for(1ms);
  ASSERT_FALSE(checkpointReached[GAMMA]);

  // DELTA now unlocks the lock that BETA is holding
  // That this is allowed sets UnshackledMutex apart from other mutexes.
  RUN(DELTA) {
    testee.unlock();
    checkpointReached[DELTA] = true;
  };

  waitUntilAtMost([&]() -> bool { return checkpointReached[DELTA]; }, 1us, 1s);

  // As DELTA has unlocked the mutex, GAMMA is now able to obtain the lock
  waitUntilAtMost([&]() -> bool { return checkpointReached[GAMMA]; }, 1us, 1s);

  // GAMMA holds the lock now
  ASSERT_FALSE(testee.try_lock());

  // EPSILON now unlocks the lock that GAMMA is holding
  RUN(EPSILON) {
    testee.unlock();
    checkpointReached[EPSILON] = true;
  };

  waitUntilAtMost([&]() -> bool { return checkpointReached[EPSILON]; }, 1us,
                  1s);

  ASSERT_TRUE(testee.try_lock());
  testee.unlock();

#undef RUN
}
