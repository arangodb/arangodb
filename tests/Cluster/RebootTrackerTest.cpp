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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Cluster/RebootTracker.h"
#include "Logger/Logger.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Random/RandomGenerator.h"

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
// MSVC new/malloc only guarantees 8 byte alignment, but SupervisedScheduler
// needs 64. Disable warning:
#if (_MSC_VER >= 1)
#pragma warning(push)
#pragma warning(disable : 4316)  // Object allocated on the heap may not be
                                 // aligned for this type
#endif
  RebootTrackerTest()
      : mockApplicationServer(),
        scheduler(std::make_unique<SupervisedScheduler>(
            mockApplicationServer.server(), 2, 64, 128, 1024 * 1024, 4096, 4096,
            128, 0.0)) {}
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

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


// Test that a registered callback with the new RebootId is not triggered
// on change
TEST_F(RebootTrackerTest, one_server_do_not_call_with_new_rebootid) {
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

    // Register callback with newer RebootId
    guard = rebootTracker.callMeOnChange({serverA, RebootId{2}}, callback, "");
    guards.emplace_back(std::move(guard));
    waitForSchedulerEmpty();
    EXPECT_EQ(0, numCalled) << "Callback must not be called before a change";

    // Now set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(1, numCalled) << "Only old callback must be called after a change";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(2, numCalled) << "Now the newer callback must be called after a change";

    guards.clear();
    EXPECT_EQ(2, numCalled)
        << "Callback must not be called when guards are destroyed";
  }
  // RebootTracker was destroyed now

  waitForSchedulerEmpty();
  EXPECT_EQ(2, numCalled) << "Callback must not be called during destruction";
}

// Test that a registered callback with the new RebootId is not triggered
// on change
TEST_F(RebootTrackerTest, one_server_simulate_high_load_many_reboots) {
  auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
      {serverA, ServerHealthState{.rebootId = RebootId{1},
                                  .status = ServerHealth::kGood}}};

  uint64_t numCalled = 0;

  uint64_t initialLevel1 = 10791000;
  uint64_t initialLevel2 = 8351371;
  uint64_t initialLevel3 = 24923155;
  uint64_t additionalLevel1 = 1287856;

  {
    RebootTracker rebootTracker{scheduler.get()};
    auto registerGuard = [&rebootTracker, &numCalled](RebootId rebootId) -> CallbackGuard {
      auto callback = [&numCalled]() { ++numCalled; };
        return rebootTracker.callMeOnChange({serverA, rebootId}, callback, "");
    };
    std::vector<CallbackGuard> guards{};
    // Set state to { serverA => 1 }
    rebootTracker.updateServerState(state);

    for (uint64_t i = 0; i < initialLevel1; ++i) {
      guards.emplace_back(registerGuard(RebootId{1}));
    }

    for (uint64_t i = 0; i < initialLevel2; ++i) {
      guards.emplace_back(registerGuard(RebootId{2}));
    }

    for (uint64_t i = 0; i < initialLevel3; ++i) {
      guards.emplace_back(registerGuard(RebootId{3}));
    }

    for (uint64_t i = 0; i < additionalLevel1; ++i) {
      guards.emplace_back(registerGuard(RebootId{1}));
    }

    // Set state to { serverA => 2 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{2},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);

    waitForSchedulerEmpty();
    EXPECT_EQ(initialLevel1 + additionalLevel1, numCalled) << "All RebootID 1 cases have to be executed";

    // Set state to { serverA => 3 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{3},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);

    waitForSchedulerEmpty();
    EXPECT_EQ(initialLevel1 + additionalLevel1 + initialLevel2, numCalled) << "All RebootID 1 and now all 2 cases have to be executed";

    // Set state to { serverA => 4 }
    state.insert_or_assign(serverA,
                           ServerHealthState{.rebootId = RebootId{4},
                                             .status = ServerHealth::kGood});
    rebootTracker.updateServerState(state);
    waitForSchedulerEmpty();
    EXPECT_EQ(initialLevel1 + additionalLevel1 + initialLevel2 + initialLevel3, numCalled) << "All RebootID 1, 2 and 3 cases have to be executed";
  }
  waitForSchedulerEmpty();
  EXPECT_EQ(initialLevel1 + additionalLevel1 + initialLevel2 + initialLevel3, numCalled) << "Callback must not be called during destruction";
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

class FuzzyRebootTrackerTest
    : public ::testing::Test,
      public LogSuppressor<Logger::CLUSTER, LogLevel::WARN> {
 protected:
// MSVC new/malloc only guarantees 8 byte alignment, but SupervisedScheduler
// needs 64. Disable warning:
#if (_MSC_VER >= 1)
#pragma warning(push)
#pragma warning(disable : 4316)  // Object allocated on the heap may not be
                                 // aligned for this type
#endif
  FuzzyRebootTrackerTest()
      : mockApplicationServer(),
        scheduler(std::make_unique<SupervisedScheduler>(
            mockApplicationServer.server(), 2, 64, 128, 1024 * 1024, 4096, 4096,
            128, 0.0)) {}
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

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

  // TODO: Implement those actions.
  // i envison a "run" method that has specific implementation for every action.
  // Maybe this Run method requires some Input (e.g. a reference to the list fo callbacks)
  struct Action {};

  struct ActionReboot : public Action {
    ServerID server;
    RebootId rebootId;
  };

  struct ActionScheduleCallback : public Action {
    ServerID server;
    RebootId rebootId;
    std::function<void()> callback;
  };

  struct ActionCancelCallback : public Action {
    ServerID server;
    RebootId rebootId;
  };

  struct ActionValidate : public Action {};

  struct Simulation {
    Simulation(double chanceToReboot, double chanceToUnregister)
         {
      init(chanceToReboot, chanceToUnregister);
      // Randomly draw a seed
      _seed = RandomGenerator::interval(std::numeric_limits<uint64_t>::max());
      // Retain the seed for reporting
      RandomGenerator::seed(_seed);
    }

    Simulation(double chanceToReboot, double chanceToUnregister, uint64_t seed)
        :
        _seed{seed} {
      init(chanceToReboot, chanceToUnregister);
      RandomGenerator::seed(_seed);
    }

    auto nextAction() -> Action {
      auto rand = RandomGenerator::interval(std::numeric_limits<uint64_t>::max());
      if (rand < _belowToReboot) {
        // TODO Randomize Action
        // TODO: Increment rebootId per server
        return ActionReboot {
          .server = ServerID{"PRMR_A"}, .rebootId = RebootId{1}
        };
      }
      if (rand < _belowToUnregister) {
        // TODO: Randomze Action, we actually need to pick a specific callback here
        return ActionCancelCallback {
            .server = ServerID{"PRMR_A"}, .rebootId = RebootId{1}
        };
      }
      // TODO: Randomize Server Selection and RebootId.
      // Define desired callback.
      return ActionScheduleCallback{
        .server = ServerID{"PRMR_A"}, .rebootId = RebootId{1}, .callback = [](){}
      };
    }

   private:
    void init(double chanceToReboot, double chanceToUnregister) {
      RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
      RandomGenerator::ensureDeviceIsInitialized();
      _belowToReboot = std::min(
          1ul, static_cast<uint64_t>(
                   chanceToReboot *
                   static_cast<double>(std::numeric_limits<uint64_t>::max())));
      _belowToUnregister = std::min(
          2ul, static_cast<uint64_t>(
                   (chanceToReboot + chanceToUnregister) *
                   static_cast<double>(std::numeric_limits<uint64_t>::max())));
    }

    uint64_t _belowToReboot;
    uint64_t _belowToUnregister;
    uint64_t _seed;
  };
};

TEST_F(FuzzyRebootTrackerTest, "simulate_cluster") {
  uint64_t nrActions = 1000;
  Simulation sim{0.001, 0.45};
  for (uint64_t i = 0; i < nrActions; ++i) {
    sim.nextAction();
    // TODO Run the Action
    // TODO: Validate from time to time e.g. every 1000 actions.
  }
}

#if true
TEST_F(RebootTrackerTest, fuzzy_test) {
    auto state = containers::FlatHashMap<ServerID, ServerHealthState>{
        {serverA, ServerHealthState{.rebootId = RebootId{0},
                                    .status = ServerHealth::kGood}}};

    std::unordered_map<uint64_t, uint64_t> expectedCalls{};
    std::unordered_map<uint64_t, uint64_t> receivedCalls{};
    uint64_t currentRebootId = 0;
    uint64_t maxRebootId = 10;
    uint64_t testScaling = 1000;
    uint64_t chanceForCompletion = 4; // out of 10 requests

    {
        RebootTracker rebootTracker{scheduler.get()};
        std::vector<std::pair<uint64_t, CallbackGuard>> guards{};
        // Set state to { serverA => 1 }
        rebootTracker.updateServerState(state);

        auto registerRandomCallback = [&]() {
          uint64_t myRebootId{std::rand() % maxRebootId + currentRebootId};
          auto callback = [&receivedCalls, myRebootId=myRebootId]() {
            receivedCalls[myRebootId]++; };
          expectedCalls[myRebootId]++;
          guards.emplace_back(std::make_pair(
              myRebootId,
              rebootTracker.callMeOnChange({serverA, RebootId{myRebootId}}, callback, "")));
        };

        auto unregisterRandomCallback = [&]() {
          if (guards.empty()) {
            return;
          }

          auto index = std::rand() % guards.size();
          auto const& rebootId = guards[index].first;
          if (rebootId >= currentRebootId) {
            // If we delete before we are triggered by reboot tracker
            // Then this callback will not be triggered
            expectedCalls[rebootId]--;
          }
          guards.erase(guards.begin() + index);
        };

        while (currentRebootId < maxRebootId) {
      auto randomNumber = std::rand() % (10 * testScaling);
      if (randomNumber == 0) {
        LOG_DEVEL << "Simulate a reboot";
        currentRebootId++;
        state.insert_or_assign(serverA,
                               ServerHealthState{.rebootId = RebootId{currentRebootId},
                                                 .status = ServerHealth::kGood});
        rebootTracker.updateServerState(state);
        waitForSchedulerEmpty();
        for (uint64_t rebootId = 0; rebootId < maxRebootId; ++rebootId) {
          if (rebootId < currentRebootId) {
            ASSERT_EQ(receivedCalls[rebootId], expectedCalls[rebootId])
                << "Checking RebootId " << rebootId
                << " for current: " << currentRebootId
                << " received unexpected amount of calls";
          } else {
            ASSERT_EQ(receivedCalls[rebootId], 0)
                << "Should not automatically trigger higher reboot ids, "
                   "checking:"
                << rebootId << " current: " << currentRebootId;
          }
        }
      } else if (randomNumber < chanceForCompletion * testScaling) {
        unregisterRandomCallback();
      } else {
        registerRandomCallback();
      }
        }
    }
}
#endif