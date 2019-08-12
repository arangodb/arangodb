////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"

#include "Cluster/RebootTracker.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include "Mocks/Servers.h"

#include "lib/Logger/Logger.h"

#include <memory>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

using rest::Scheduler;

TEST_CASE("CallbackGuardTest") {
  auto counterA = uint64_t{0};
  auto counterB = uint64_t{0};
  auto incrCounterA = [&counterA]() { ++counterA; };
  auto incrCounterB = [&counterB]() { ++counterB; };

  SECTION("test_default_constructor") {
    // should do nothing, especially not cause an error during destruction
    CallbackGuard guard{};
  }

  SECTION("test_deleted_copy_semantics") {
    {
      INFO("CallbackGuard should not be copy constructible");
      CHECK_FALSE(std::is_copy_constructible<CallbackGuard>::value);
    }
    {
      INFO("CallbackGuard should not be copy assignable");
      CHECK_FALSE(std::is_copy_assignable<CallbackGuard>::value);
    }
  }

  SECTION("test_constructor") {
    {
      CallbackGuard guard{incrCounterA};
      {
        INFO("construction should not invoke the callback");
        CHECK(0 == counterA);
      }
    }
    {
      INFO("destruction should invoke the callback");
      CHECK(1 == counterA);
    }
  }

  SECTION("test_move_constructor_inline") {
    {
      CallbackGuard guard{CallbackGuard(incrCounterA)};
      {
        INFO("move construction should not invoke the callback");
        CHECK(0 == counterA);
      }
    }
    {
      INFO("destruction should invoke the callback");
      CHECK(1 == counterA);
    }
  }

  SECTION("test_move_constructor_explicit") {
    {
      CallbackGuard guardA1{incrCounterA};
      {
        INFO("construction should not invoke the callback");
        CHECK(0 == counterA);
      }
      {
        CallbackGuard guardA2{std::move(guardA1)};
        {
          INFO("move construction should not invoke the callback");
          CHECK(0 == counterA);
        }
      }
      {
        INFO("destroying a move constructed guard should invoke the callback");
        CHECK(1 == counterA);
      }
    }

    {
      INFO("destroying a moved guard should not invoke the callback");
      CHECK(1 == counterA);
    }
  }

  SECTION("test_move_operator_eq_construction") {
    {
      auto guard = CallbackGuard{incrCounterA};
      {
        INFO("initialization with operator= should not invoke the callback");
        CHECK(0 == counterA);
      }
    }
    {
      INFO("destruction should invoke the callback");
      CHECK(1 == counterA);
    }
  }

  SECTION("test_move_operator_eq_explicit") {
    {
      CallbackGuard guardA{incrCounterA};
      {
        INFO("construction should not invoke the callback");
        CHECK(0 == counterA);
      }
      {
        CallbackGuard guardB{incrCounterB};
        {
          INFO("construction should not invoke the callback");
          CHECK(0 == counterB);
        }
        guardA = std::move(guardB);
        {
          INFO("being moved should not invoke the callback");
          CHECK(0 == counterB);
        }
        {
          INFO("being overwritten should invoke the callback");
          CHECK(1 == counterA);
        }
      }
      {
        INFO("destroying a moved guard should not invoke the callback");
        CHECK(0 == counterB);
      }
      {
        INFO(
            "destroying a moved guard should not invoke the "
            "overwritten callback again");
        CHECK(1 == counterA);
      }
    }
    {
      INFO("destroying an overwritten guard should invoke its new callback");
      CHECK(1 == counterB);
    }
    {
      INFO(
          "destroying an overwritten guard should not invoke "
          "its old callback again");
      CHECK(1 == counterA);
    }
  }
}

TEST_CASE("RebootTrackerTest") {
  ServerID const serverA = "PRMR-srv-A";
  ServerID const serverB = "PRMR-srv-B";
  ServerID const serverC = "PRMR-srv-C";

  using PeerState = RebootTracker::PeerState;

  auto scheduler = std::make_unique<Scheduler>(2, 64, 1024 * 1024, 4096);
  static_assert(std::is_same<decltype(*SchedulerFeature::SCHEDULER), decltype(*scheduler)>::value,
                "Use the correct scheduler in the tests");
  // ApplicationServer needs to be prepared in order for the scheduler to start
  // threads.
  MockEmptyServer mockApplicationServer;

  // Suppress this INFO message:
  // When trying to register callback '': The server PRMR-srv-A is not known. If this server joined the cluster in the last seconds, this can happen.
  arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::WARN);

  TRI_DEFER(scheduler->shutdown());
  scheduler->start();

  auto schedulerEmpty = [&]() -> bool {
    auto stats = scheduler->queueStatistics();

    return stats._running == 0 && stats._queued == 0 && stats._working == 0;
  };

  auto waitForSchedulerEmpty = [&]() {
    while (!schedulerEmpty()) {
      std::this_thread::yield();
    }
  };

  // Test that a registered callback is called once on the next change, but not
  // after that
  SECTION("one_server_call_once_after_change") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      std::vector<CallbackGuard> guards{};
      CallbackGuard guard;

      // Set state to { serverA => 1 }
      rebootTracker.updateServerState(state);

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called twice");
        CHECK(1 == numCalled);
      }

      guards.clear();
      {
        INFO("Callback must not be called when guards are destroyed");
        CHECK(1 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(1 == numCalled);
    }
  }

  // Test that a registered callback is called immediately when its reboot id
  // is lower than the last known one, but not after that
  SECTION("one_server_call_once_with_old_rebootid") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{2}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      std::vector<CallbackGuard> guards{};
      CallbackGuard guard;

      // Set state to { serverA => 2 }
      rebootTracker.updateServerState(state);

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback with lower value must be called immediately");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called again");
        CHECK(1 == numCalled);
      }

      guards.clear();
      {
        INFO("Callback must not be called when guards are destroyed");
        CHECK(1 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(1 == numCalled);
    }
  }

  // Tests that callbacks and interleaved updates don't interfere
  SECTION("one_server_call_interleaved") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      std::vector<CallbackGuard> guards{};
      CallbackGuard guard;

      // Set state to { serverA => 1 }
      rebootTracker.updateServerState(state);

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called twice");
        CHECK(1 == numCalled);
      }

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{3}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 4 }
      state.at(serverA) = RebootId{4};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(2 == numCalled);
      }

      // Set state to { serverA => 5 }
      state.at(serverA) = RebootId{5};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called twice");
        CHECK(2 == numCalled);
      }

      guards.clear();
      {
        INFO("Callback must not be called when guards are destroyed");
        CHECK(2 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(2 == numCalled);
    }
  }

  // Tests that multiple callbacks and updates don't interfere
  SECTION("one_server_call_sequential") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      std::vector<CallbackGuard> guards{};
      CallbackGuard guard;

      // Set state to { serverA => 1 }
      rebootTracker.updateServerState(state);

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Both callbacks must be called after a change");
        CHECK(2 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("No callback must be called twice");
        CHECK(2 == numCalled);
      }

      guards.clear();
      {
        INFO("Callback must not be called when guards are destroyed");
        CHECK(2 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(2 == numCalled);
    }
  }

  // Test that a registered callback is removed when its guard is destroyed
  SECTION("one_server_guard_removes_callback") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};

      // Set state to { serverA => 1 }
      rebootTracker.updateServerState(state);

      {
        // Register callback
        auto guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                                  callback, "");
        waitForSchedulerEmpty();
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == numCalled);
        }
      }
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called when the guard is destroyed");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called after a change "
            "when the guard was destroyed before");
        CHECK(0 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(0 == numCalled);
    }
  }

  // Test that callback removed by a guard doesn't interfere with other
  // registered callbacks for the same server and reboot id
  SECTION("one_server_guard_doesnt_interfere") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

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
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           incrCounterA, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == counterA);
      }

      {
        // Register callback with a local guard
        auto localGuard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                                       incrCounterB, "");
        waitForSchedulerEmpty();
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == counterA);
        }
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == counterB);
        }

        // Register callback
        guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                             incrCounterC, "");
        guards.emplace_back(std::move(guard));
        waitForSchedulerEmpty();
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == counterA);
        }
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == counterB);
        }
        {
          INFO("Callback must not be called before a change");
          CHECK(0 == counterC);
        }
      }
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called when the guard is destroyed");
        CHECK(0 == counterA);
      }
      {
        INFO("Callback must not be called when the guard is destroyed");
        CHECK(0 == counterB);
      }
      {
        INFO("Callback must not be called when the guard is destroyed");
        CHECK(0 == counterC);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(1 == counterA);
      }
      {
        INFO("Removed callback must not be called after a change");
        CHECK(0 == counterB);
      }
      {
        INFO("Callback must be called after a change");
        CHECK(1 == counterC);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("No callback must be called twice");
        CHECK(1 == counterA);
      }
      {
        INFO("Removed callback must not be called after a change");
        CHECK(0 == counterB);
      }
      {
        INFO("No callback must be called twice");
        CHECK(1 == counterC);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(1 == counterA);
    }
    {
      INFO("Callback must not be called during destruction");
      CHECK(0 == counterB);
    }
    {
      INFO("Callback must not be called during destruction");
      CHECK(1 == counterC);
    }
  }

  SECTION("one_server_add_callback_before_state_with_same_id") {
    auto state = std::unordered_map<ServerID, RebootId>{};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      CallbackGuard guard;

      // State is empty { }

      // Register callback
      {
        INFO(
            "Trying to add a callback for an unknown server should be refused");
        CHECK_THROWS_AS(guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                                             callback, ""),
                        arangodb::basics::Exception);
      }
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 1 }
      state.emplace(serverA, RebootId{1});
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called when the state is "
            "set to the same RebootId, as it shouldn't have been registered");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called after a change, as "
            "it shouldn't have been registered");
        CHECK(0 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(0 == numCalled);
    }
  }

  SECTION("one_server_add_callback_before_state_with_older_id") {
    auto state = std::unordered_map<ServerID, RebootId>{};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      CallbackGuard guard;

      // State is empty { }

      // Register callback
      {
        INFO(
            "Trying to add a callback for an unknown server should be refused");
        CHECK_THROWS_AS(guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{2}},
                                                             callback, ""),
                        arangodb::basics::Exception);
      }
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 1 }
      state.emplace(serverA, RebootId{1});
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called when the state is "
            "set to an older RebootId");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called when the state is "
            "set to the same RebootId");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO(
            "Callback must not be called after a change, as "
            "it shouldn't have been registered");
        CHECK(0 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(0 == numCalled);
    }
  }

  SECTION("two_servers_call_interleaved") {
    auto state = std::unordered_map<ServerID, RebootId>{{serverA, RebootId{1}}};

    uint64_t numCalled = 0;
    auto callback = [&numCalled]() { ++numCalled; };

    {
      RebootTracker rebootTracker{scheduler.get()};
      std::vector<CallbackGuard> guards{};
      CallbackGuard guard;

      // Set state to { serverA => 1 }
      rebootTracker.updateServerState(state);

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{1}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(0 == numCalled);
      }

      // Set state to { serverA => 2 }
      state.at(serverA) = RebootId{2};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 3 }
      state.at(serverA) = RebootId{3};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called twice");
        CHECK(1 == numCalled);
      }

      // Register callback
      guard = rebootTracker.callMeOnChange(PeerState{serverA, RebootId{3}},
                                           callback, "");
      guards.emplace_back(std::move(guard));
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called before a change");
        CHECK(1 == numCalled);
      }

      // Set state to { serverA => 4 }
      state.at(serverA) = RebootId{4};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must be called after a change");
        CHECK(2 == numCalled);
      }

      // Set state to { serverA => 5 }
      state.at(serverA) = RebootId{5};
      rebootTracker.updateServerState(state);
      waitForSchedulerEmpty();
      {
        INFO("Callback must not be called twice");
        CHECK(2 == numCalled);
      }

      guards.clear();
      {
        INFO("Callback must not be called when guards are destroyed");
        CHECK(2 == numCalled);
      }
    }
    // RebootTracker was destroyed now

    waitForSchedulerEmpty();
    {
      INFO("Callback must not be called during destruction");
      CHECK(2 == numCalled);
    }
  }
}
