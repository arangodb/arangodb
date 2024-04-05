////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/LeaseManager/LeaseManager.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/RebootTracker.h"
#include "Metrics/MetricsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"

#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::tests::mocks;

class LeaseManagerTest : public ::testing::Test {
 protected:
  LeaseManagerTest()
      : mockApplicationServer(),
        scheduler(std::make_unique<SupervisedScheduler>(
            mockApplicationServer.server(), 2, 64, 128, 1024 * 1024, 4096, 4096,
            128, 0.0, 42,
            mockApplicationServer.server()
                .template getFeature<arangodb::metrics::MetricsFeature>())),
        rebootTracker(scheduler.get()) {}

  MockRestServer mockApplicationServer;
  std::unique_ptr<SupervisedScheduler> scheduler;
  RebootTracker rebootTracker;

  static ServerID const serverA;
  static ServerID const serverB;
  static ServerID const serverC;

  containers::FlatHashMap<ServerID, ServerHealthState> state;

  // ApplicationServer needs to be prepared in order for the scheduler to start
  // threads.

  void SetUp() override {
    scheduler->start();
    state = containers::FlatHashMap<ServerID, ServerHealthState>{
        {serverA, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}},
        {serverB, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}},
        {serverC, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}}};
    rebootTracker.updateServerState(state);
  }
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

  void rebootServer(ServerID const& server) {
    auto it = state.find(server);
    ASSERT_NE(it, state.end())
        << "Test setup incorrect, tried to reboot a server that does not "
           "participate in the test.";
    it->second.rebootId = RebootId{it->second.rebootId.value() + 1};
    rebootTracker.updateServerState(state);
    // Need to wait for the scheduler to actually work on the RebootTracker.
    waitForSchedulerEmpty();
  }

  auto getPeerState(ServerID const& server) -> PeerState {
    auto it = state.find(server);
    TRI_ASSERT(it != state.end())
        << "Test setup incorrect, tried to getPeerState for a server that does "
           "not participate in the test.";

    return PeerState{.serverId = server, .rebootId = it->second.rebootId};
  }

  auto peerStateToJSONKey(const PeerState& peerState) {
    return fmt::format("{}:{}", peerState.serverId,
                       basics::StringUtils::itoa(peerState.rebootId.value()));
  }

  auto assertLeaseListContainsLease(VPackSlice leasesVPack,
                                    PeerState const& leaseIsFor,
                                    LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey(peerStateToJSONKey(leaseIsFor)))
        << "LeaseManager should have an entry for the server "
        << peerStateToJSONKey(leaseIsFor)
        << " full list: " << leasesVPack.toJson();
    auto leaseList = leasesVPack.get(peerStateToJSONKey(leaseIsFor));
    EXPECT_TRUE(leaseList.hasKey(basics::StringUtils::itoa(leaseId.id())))
        << "LeaseManager should have an entry for the lease " << leaseId
        << " full list: " << leaseList.toJson();
  }

  auto assertLeaseListDoesNotContainLease(VPackSlice leasesVPack,
                                          PeerState const& leaseIsFor,
                                          LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    if (auto leaseList = leasesVPack.get(peerStateToJSONKey(leaseIsFor));
        !leaseList.isNone()) {
      EXPECT_FALSE(leaseList.hasKey(basics::StringUtils::itoa(leaseId.id())))
          << "LeaseManager should not have an entry for the lease " << leaseId
          << " full list: " << leaseList.toJson();
    }
    // Else case okay if we have no entry for the server, we cannot have an
    // entry for the lease.
  }
};

ServerID const LeaseManagerTest::serverA = "PRMR-srv-A";
ServerID const LeaseManagerTest::serverB = "PRMR-srv-B";
ServerID const LeaseManagerTest::serverC = "PRMR-srv-C";

TEST_F(LeaseManagerTest, test_every_lease_has_a_unique_id) {
  LeaseManager leaseManager{rebootTracker};
  auto leaseIsFor = getPeerState(serverA);
  auto ignoreMe = []() noexcept -> void {};
  auto guardOne = leaseManager.requireLease(leaseIsFor, ignoreMe);
  auto guardTwo = leaseManager.requireLease(leaseIsFor, ignoreMe);
  EXPECT_NE(guardOne.id(), guardTwo.id());
  auto leasesVPackBuilder = leaseManager.leasesToVPack();
  auto leasesVPack = leasesVPackBuilder.slice();
  LOG_DEVEL << leasesVPack.toJson();
  ASSERT_TRUE(leasesVPack.isObject());
  EXPECT_EQ(leasesVPack.length(), 1);
  ASSERT_TRUE(leasesVPack.hasKey(peerStateToJSONKey(leaseIsFor)));
  auto leaseList = leasesVPack.get(peerStateToJSONKey(leaseIsFor));
  EXPECT_TRUE(leaseList.hasKey(basics::StringUtils::itoa(guardOne.id().id())))
      << "LeaseManager should have a key for the first lease " << guardOne.id()
      << " full list: " << leasesVPack.toJson();
  EXPECT_TRUE(leaseList.hasKey(basics::StringUtils::itoa(guardTwo.id().id())))
      << "LeaseManager should have a key for the second lease " << guardTwo.id()
      << " full list: " << leasesVPack.toJson();
}

TEST_F(LeaseManagerTest, test_lease_is_removed_from_list_on_guard_destruction) {
  bool rebootCallbackCalled = false;
  LeaseManager leaseManager{rebootTracker};
  auto leaseIsFor = getPeerState(serverA);
  LeaseId storedId;
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.requireLease(leaseIsFor, callback);
    storedId = lease.id();
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeaseListContainsLease(leaseReport.slice(), leaseIsFor, lease.id());
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeaseListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                       storedId);
  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  waitForSchedulerEmpty();
  EXPECT_TRUE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_can_cancel_abort_callback) {
  bool rebootCallbackCalled = false;
  LeaseManager leaseManager{rebootTracker};
  auto leaseIsFor = getPeerState(serverA);
  LeaseId storedId;
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.requireLease(leaseIsFor, callback);
    storedId = lease.id();
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeaseListContainsLease(leaseReport.slice(), leaseIsFor, lease.id());
    lease.cancel();
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeaseListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                       storedId);
  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  // (Should not do anything here)
  waitForSchedulerEmpty();
  EXPECT_FALSE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_is_aborted_on_peer_reboot) {
  {
    bool rebootCallbackCalled = false;
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    LeaseManager leaseManager{rebootTracker};
    auto leaseIsFor = getPeerState(serverA);
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    [[maybe_unused]] auto lease =
        leaseManager.requireLease(leaseIsFor, callback);
    rebootServer(serverA);
    EXPECT_TRUE(rebootCallbackCalled);
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeaseListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                       lease.id());
    // In fact we should not even have a server entry anymore.
    EXPECT_FALSE(leaseReport.slice().hasKey(peerStateToJSONKey(leaseIsFor)));
  }
}

TEST_F(LeaseManagerTest, test_canceled_lease_is_not_aborted_on_peer_reboot) {
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    LeaseManager leaseManager{rebootTracker};
    auto leaseIsFor = getPeerState(serverA);
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    [[maybe_unused]] auto lease =
        leaseManager.requireLease(leaseIsFor, callback);
    lease.cancel();
    {
      // Cancel does not take the Lease out of the list!
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeaseListContainsLease(leaseReport.slice(), leaseIsFor, lease.id());
    }
    rebootServer(serverA);
    // NOTE: Lease is still in Scope, but the callback should not be called.
    {
      // Rebooting the server does remove the lease from the list.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeaseListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                         lease.id());
      // In fact we should not even have a server entry anymore.
      EXPECT_FALSE(leaseReport.slice().hasKey(peerStateToJSONKey(leaseIsFor)));
    }
    EXPECT_FALSE(rebootCallbackCalled)
        << "Called callback on canceled lease if server rebooted";
  }
  EXPECT_FALSE(rebootCallbackCalled)
      << "Called callback on canceled lease if guard goes out of scope";
}

