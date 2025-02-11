////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 ArangoDB GmbH, Cologne, Germany
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

#include <latch>
#include "gtest/gtest.h"

#include "Mocks/Servers.h"

#include "Scheduler/SupervisedScheduler.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

struct SupervisedSchedulerTest : ::testing::Test {
  SupervisedSchedulerTest() : mockApplicationServer() {}
  MockRestServer mockApplicationServer;
};

struct NonSchedulerDetachingThread : Thread {
  explicit NonSchedulerDetachingThread(ArangodServer& server,
                                       SupervisedScheduler* scheduler,
                                       std::latch* threadReady)
      : Thread(server, "NonSchedulerDetachingThread"),
        _scheduler(scheduler),
        _threadReady(threadReady) {}

 protected:
  void run() override {
    _threadReady->count_down();
    while (!isStopping()) {
      _scheduler->detachThread(nullptr, nullptr);
    }
  }

 private:
  SupervisedScheduler* _scheduler{};
  std::latch* _threadReady;
};

// There was a race between starting a thread, which writes its own
// Thread::_threadNumber, and another thread calling detachThread, which
// iterates over all threads and compares their _threadNumber against its own.
// This should now be handled
TEST_F(SupervisedSchedulerTest, regression_test_bts_2078) {
  using namespace std::chrono_literals;

  constexpr std::uint64_t minThreads = 256;
  auto scheduler = std::make_unique<SupervisedScheduler>(
      mockApplicationServer.server(), minThreads, 256, 128, 1024 * 1024, 4096,
      4096, 128, 0.0, 42);

  std::latch threadReady(1);
  auto detachingThread = NonSchedulerDetachingThread(
      mockApplicationServer.server(), scheduler.get(), &threadReady);
  detachingThread.start();
  threadReady.wait();
  std::this_thread::sleep_for(100ns);
  scheduler->start();

  while (scheduler->queueStatistics()._running < minThreads) {
    std::this_thread::sleep_for(100ns);
  }
  detachingThread.shutdown();
  scheduler->shutdown();
}
