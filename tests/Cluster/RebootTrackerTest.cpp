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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Cluster/RebootTracker.h"
#include "Logger/Logger.h"
#include "Metrics/MetricsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

class CallbackGuardTest : public ::testing::Test {
 protected:
  uint64_t counterA{0};
  uint64_t counterB{0};
  fu2::function<void() noexcept> incrCounterA;
  fu2::function<void() noexcept> incrCounterB;

  void SetUp() override {
    counterA = 0;
    counterB = 0;
    incrCounterA = [&counterA = this->counterA]() noexcept { ++counterA; };
    incrCounterB = [&counterB = this->counterB]() noexcept { ++counterB; };
  }
};

TEST_F(CallbackGuardTest, test_default_constructor) {
  // should do nothing, especially not cause an error during destruction
  CallbackGuard guard{};
}

TEST_F(CallbackGuardTest, test_deleted_copy_semantics) {
  EXPECT_FALSE(std::is_copy_constructible<CallbackGuard>::value)
      << "CallbackGuard should not be copy constructible";
  EXPECT_FALSE(std::is_copy_assignable<CallbackGuard>::value)
      << "CallbackGuard should not be copy assignable";
}

TEST_F(CallbackGuardTest, test_constructor) {
  {
    CallbackGuard guard{incrCounterA};
    EXPECT_EQ(0, counterA) << "construction should not invoke the callback";
  }
  EXPECT_EQ(1, counterA) << "destruction should invoke the callback";
}

TEST_F(CallbackGuardTest, test_move_constructor_inline) {
  {
    CallbackGuard guard{CallbackGuard(incrCounterA)};
    EXPECT_EQ(0, counterA)
        << "move construction should not invoke the callback";
  }
  EXPECT_EQ(1, counterA) << "destruction should invoke the callback";
}

TEST_F(CallbackGuardTest, test_move_constructor_explicit) {
  {
    CallbackGuard guardA1{incrCounterA};
    EXPECT_EQ(0, counterA) << "construction should not invoke the callback";
    {
      CallbackGuard guardA2{std::move(guardA1)};
      EXPECT_EQ(0, counterA)
          << "move construction should not invoke the callback";
    }
    EXPECT_EQ(1, counterA)
        << "destroying a move constructed guard should invoke the callback";
  }

  EXPECT_EQ(1, counterA)
      << "destroying a moved guard should not invoke the callback";
}

TEST_F(CallbackGuardTest, test_move_operator_eq_construction) {
  {
    auto guard = CallbackGuard{incrCounterA};
    EXPECT_EQ(0, counterA)
        << "initialization with operator= should not invoke the callback";
  }
  EXPECT_EQ(1, counterA) << "destruction should invoke the callback";
}

TEST_F(CallbackGuardTest, test_move_operator_eq_explicit) {
  {
    CallbackGuard guardA{incrCounterA};
    EXPECT_EQ(0, counterA) << "construction should not invoke the callback";
    {
      CallbackGuard guardB{incrCounterB};
      EXPECT_EQ(0, counterB) << "construction should not invoke the callback";
      guardA = std::move(guardB);
      EXPECT_EQ(0, counterB) << "being moved should not invoke the callback";
      EXPECT_EQ(1, counterA) << "being overwritten should invoke the callback";
    }
    EXPECT_EQ(0, counterB)
        << "destroying a moved guard should not invoke the callback";
    EXPECT_EQ(1, counterA) << "destroying a moved guard should not invoke the "
                              "overwritten callback again";
  }
  EXPECT_EQ(1, counterB)
      << "destroying an overwritten guard should invoke its new callback";
  EXPECT_EQ(1, counterA) << "destroying an overwritten guard should not invoke "
                            "its old callback again";
}

class RebootTrackerTest
    : public ::testing::Test,
      public LogSuppressor<Logger::CLUSTER, LogLevel::WARN> {
 protected:
  RebootTrackerTest()
      : mockApplicationServer(),
        scheduler(std::make_unique<SupervisedScheduler>(
            mockApplicationServer.server(), 2, 64, 128, 1024 * 1024, 4096, 4096,
            128, 0.0,
            std::make_shared<SchedulerMetrics>(
                mockApplicationServer.server()
                    .template getFeature<
                        arangodb::metrics::MetricsFeature>()))) {}

  MockRestServer mockApplicationServer;
  std::unique_ptr<SupervisedScheduler> scheduler;

  // ApplicationServer needs to be prepared in order for the scheduler to start
  // threads.

  void SetUp() override { scheduler->start(); }
  void TearDown() override { scheduler->shutdown(); }

  bool schedulerEmpty() const {
    auto stats = scheduler->queueStatistics();

    return stats._queued == 0 && stats._working == 0;
  }

  void waitForSchedulerEmpty() const {
    while (!schedulerEmpty()) {
      std::this_thread::yield();
    }
  }

  static ServerID const serverA;
  static ServerID const serverB;
  static ServerID const serverC;
};

ServerID const RebootTrackerTest::serverA = "PRMR-srv-A";
ServerID const RebootTrackerTest::serverB = "PRMR-srv-B";
ServerID const RebootTrackerTest::serverC = "PRMR-srv-C";

// Test that a registered callback is called once on the next change, but not
// after that
TEST_F(RebootTrackerTest, one_server_call_once_after_change) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called twice";

    guards.clear();
    EXPECT_EQ(1, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(1, numCalled) << "Callback must not be called during destruction";
}

// Test that a registered callback is called immediately when its reboot id
// is lower than the last known one, but not after that
TEST_F(RebootTrackerTest, one_server_call_once_with_old_rebootid) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{2},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 2 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled)
        << "Callback with lower value must be called immediately";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called again";

    guards.clear();
    EXPECT_EQ(1, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(1, numCalled) << "Callback must not be called during destruction";
}

// Tests that callbacks and interleaved updates don't interfere
TEST_F(RebootTrackerTest, one_server_call_interleaved) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called twice";

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{3}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 4 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{4},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Callback must be called after a change";

    // Set state to { serverA => 5 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{5},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Callback must not be called twice";

    guards.clear();
    EXPECT_EQ(2, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(2, numCalled) << "Callback must not be called during destruction";
}

// Tests that multiple callbacks and updates don't interfere
TEST_F(RebootTrackerTest, one_server_call_sequential) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Both callbacks must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "No callback must be called twice";

    guards.clear();
    EXPECT_EQ(2, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(2, numCalled) << "Callback must not be called during destruction";
}

// Test that a registered callback is removed when its guard is destroyed
TEST_F(RebootTrackerTest, one_server_guard_removes_callback) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    {
      // Register callback
      auto guard =
          rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
      waitForSchedulerEmpty();
      EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";
    }
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled)
        << "Callback must not be called when the guard is destroyed";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called after a change "
                               "when the guard was destroyed before";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(0, numCalled) << "Callback must not be called during destruction";
}

// Test that callback removed by a guard doesn't interfere with other registered
// callbacks for the same server and reboot id
TEST_F(RebootTrackerTest, one_server_guard_doesnt_interfere) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t counterA = 0;
  uint64_t counterB = 0;
  uint64_t counterC = 0;
  auto incrCounterA = [&counterA]() { ++counterA; };
  auto incrCounterB = [&counterB]() { ++counterB; };
  auto incrCounterC = [&counterC]() { ++counterC; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard =
        rebootTracker.callMeOnChange({serverA, RebootId{1}}, incrCounterA, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, counterA) << "Callback must not be called before a change";

    {
      // Register callback with a local guard
      auto localGuard = rebootTracker.callMeOnChange({serverA, RebootId{1}},
                                                     incrCounterB, "");
      waitForSchedulerEmpty();
      EXPECT_EQ(0, counterA) << "Callback must not be called before a change";
      EXPECT_EQ(0, counterB) << "Callback must not be called before a change";

      // Register callback
      guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, incrCounterC,
                                           "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      EXPECT_EQ(0, counterA) << "Callback must not be called before a change";
      EXPECT_EQ(0, counterB) << "Callback must not be called before a change";
      EXPECT_EQ(0, counterC) << "Callback must not be called before a change";
    }
    waitForSchedulerEmpty();
    EXPECT_EQ(0, counterA)
        << "Callback must not be called when the guard is destroyed";
    EXPECT_EQ(0, counterB)
        << "Callback must not be called when the guard is destroyed";
    EXPECT_EQ(0, counterC)
        << "Callback must not be called when the guard is destroyed";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, counterA) << "Callback must be called after a change";
    EXPECT_EQ(0, counterB)
        << "Removed callback must not be called after a change";
    EXPECT_EQ(1, counterC) << "Callback must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, counterA) << "No callback must be called twice";
    EXPECT_EQ(0, counterB)
        << "Removed callback must not be called after a change";
    EXPECT_EQ(1, counterC) << "No callback must be called twice";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(1, counterA) << "Callback must not be called during destruction";
  EXPECT_EQ(0, counterB) << "Callback must not be called during destruction";
  EXPECT_EQ(1, counterC) << "Callback must not be called during destruction";
}

TEST_F(RebootTrackerTest, one_server_add_callback_before_state_with_same_id) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    CallbackGuard guard;

    // State is empty { }

    // Register callback
    EXPECT_THROW(guard = rebootTracker.callMeOnChange({serverA, RebootId{1}},
                                                      callback, ""),
                 arangodb::basics::Exception)
        << "Trying to add a callback for an unknown server should be refused";
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 1 }
    state.emplace(serverA, ServerHealthState{.rebootId = RebootId{1},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled)
        << "Callback must not be called when the state is "
           "set to the same RebootId, as it shouldn't have been registered";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called after a change, as "
                               "it shouldn't have been registered";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(0, numCalled) << "Callback must not be called during destruction";
}

TEST_F(RebootTrackerTest, one_server_add_callback_before_state_with_older_id) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    CallbackGuard guard;

    // State is empty { }

    // Register callback
    EXPECT_THROW(guard = rebootTracker.callMeOnChange({serverA, RebootId{2}},
                                                      callback, ""),
                 arangodb::basics::Exception)
        << "Trying to add a callback for an unknown server should be refused";
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 1 }
    state.emplace(serverA, ServerHealthState{.rebootId = RebootId{1},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called when the state is "
                               "set to an older RebootId";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called when the state is "
                               "set to the same RebootId";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called after a change, as "
                               "it shouldn't have been registered";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(0, numCalled) << "Callback must not be called during destruction";
}

TEST_F(RebootTrackerTest, two_servers_call_interleaved) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;
  auto callback = [&numCalled]() { ++numCalled; };

  {
    RebootTracker rebootTracker{scheduler.get()};
    std::vector<CallbackGuard> guards{};
    CallbackGuard guard;

    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{1}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called twice";

    // Register callback
    guard = rebootTracker.callMeOnChange({serverA, RebootId{3}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Callback must not be called before a change";

    // Set state to { serverA => 4 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{4},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Callback must be called after a change";

    // Set state to { serverA => 5 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{5},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Callback must not be called twice";

    guards.clear();
    EXPECT_EQ(2, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(2, numCalled) << "Callback must not be called during destruction";
}
