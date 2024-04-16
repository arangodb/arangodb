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
#include <gmock/gmock.h>

#include "Basics/voc-errors.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/LeaseManager/LeaseManager.h"
#include "Cluster/LeaseManager/LeaseManagerNetworkHandler.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/RebootTracker.h"
#include "IResearch/RestHandlerMock.h"
#include "Metrics/MetricsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"

#include "Mocks/Servers.h"
#include "PreparedResponseConnectionPool.h"

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::tests;
using namespace arangodb::tests::mocks;

struct MockLeaseManagerNetworkHandler : public ILeaseManagerNetworkHandler {
  MOCK_METHOD(futures::Future<Result>, abortIds,
              (ServerID const& server, std::vector<LeaseId> const& ids),
              (const, noexcept, override));
};

class LeaseManagerTest : public ::testing::Test {
 protected:
  LeaseManagerTest()
      : mockApplicationServer(),
        scheduler(std::make_unique<SupervisedScheduler>(
            mockApplicationServer.server(), 2, 64, 128, 1024 * 1024, 4096, 4096,
            128, 0.0, 42,
            mockApplicationServer.server()
                .template getFeature<arangodb::metrics::MetricsFeature>())),
        rebootTracker(scheduler.get()){
  }

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
  void TearDown() override {
    /* NOTE:
     * If you ever see this test failing with such a message:
     * There was still a task queued by the LeaseManager and afterwards we did not
     * call `waitForSchedulerEmpty();` Please check the failing test if this could be the case, e.g.
     * has the test waited after a reboot of the server? Has the test waited if handing in an Illegal PeerState?
     * 2024-04-05T08:55:47Z [2352775] WARNING {threads} Scheduler received shutdown, but there are still tasks on the queue: jobsSubmitted=1 jobsDone=0
     * Signal: SIGSEGV (signal SIGSEGV: invalid address (fault address: 0xf))
     */
    scheduler->shutdown(); }

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

  auto buildManager() -> LeaseManager {
    auto networkMock = std::make_unique<testing::NiceMock<MockLeaseManagerNetworkHandler>>();
    // Add default behaviour: Successfully abort all IDs.
    ON_CALL(*networkMock, abortIds).WillByDefault([&]() -> auto {
      auto promise = futures::Promise<Result>{};
      auto future = promise.getFuture();
      scheduler->queue(RequestLane::CONTINUATION, [promise = std::move(promise)]() mutable -> void {
        promise.setValue(Result{});
      });
      return future;
    });
    return LeaseManager{rebootTracker, std::move(networkMock)};
  }

  auto getNetworkMock(LeaseManager& manager) -> testing::NiceMock<MockLeaseManagerNetworkHandler>* {
    return static_cast<testing::NiceMock<MockLeaseManagerNetworkHandler>*>(
        manager.getNetworkHandler());
  }

  auto assertLeasedFromListContainsLease(VPackSlice leasesVPack,
                                         PeerState const& leaseIsFor,
                                         LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey("leasedFromRemote"));
    leasesVPack = leasesVPack.get("leasedFromRemote");
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

  auto assertLeasedFromListDoesNotContainLease(VPackSlice leasesVPack,
                                               PeerState const& leaseIsFor,
                                               LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey("leasedFromRemote"));
    leasesVPack = leasesVPack.get("leasedFromRemote");
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

  auto assertLeasedToListContainsLease(VPackSlice leasesVPack,
                                       PeerState const& leaseIsTo,
                                       LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey("leasedToRemote"));
    leasesVPack = leasesVPack.get("leasedToRemote");
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey(peerStateToJSONKey(leaseIsTo)))
        << "LeaseManager should have an entry for the server "
        << peerStateToJSONKey(leaseIsTo)
        << " full list: " << leasesVPack.toJson();
    auto leaseList = leasesVPack.get(peerStateToJSONKey(leaseIsTo));
    EXPECT_TRUE(leaseList.hasKey(basics::StringUtils::itoa(leaseId.id())))
        << "LeaseManager should have an entry for the lease " << leaseId
        << " full list: " << leaseList.toJson();
  }

  auto assertLeasedToListDoesNotContainLease(VPackSlice leasesVPack,
                                             PeerState const& leaseIsTo,
                                             LeaseId const& leaseId) {
    ASSERT_TRUE(leasesVPack.isObject());
    ASSERT_TRUE(leasesVPack.hasKey("leasedToRemote"));
    leasesVPack = leasesVPack.get("leasedToRemote");
    ASSERT_TRUE(leasesVPack.isObject());
    if (auto leaseList = leasesVPack.get(peerStateToJSONKey(leaseIsTo));
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
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  auto ignoreMe = []() noexcept -> void {};
  auto guardOne = leaseManager.requireLease(leaseIsFor, ignoreMe);
  auto guardTwo = leaseManager.requireLease(leaseIsFor, ignoreMe);
  EXPECT_NE(guardOne.id(), guardTwo.id());
  auto leaseReport = leaseManager.leasesToVPack();
  assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                    guardOne.id());
  assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                    guardTwo.id());
}

TEST_F(LeaseManagerTest, test_a_lease_from_remote_can_be_moved_around) {
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  uint64_t wasCalled = 0;
  auto countingCallback = [&]() noexcept -> void {
    wasCalled++;
  };
  auto networkMock = getNetworkMock(leaseManager);
  // We are not allowed to abort remote leases here.
  EXPECT_CALL(*networkMock, abortIds(testing::_, testing::_)).Times(0);
  struct MyStructure {
    LeaseManager::LeaseFromRemoteGuard lease;
  };
  {
    auto guardOne = leaseManager.requireLease(leaseIsFor, countingCallback);
    auto storedId = guardOne.id();
    auto myStructure = MyStructure{.lease = std::move(guardOne)};
    waitForSchedulerEmpty();
    EXPECT_EQ(wasCalled, 0) << "Callback was called while moving around.";
    // We now go out of scope with myStruct. Can call abort now.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{storedId})).Times(1);
  }
}

TEST_F(LeaseManagerTest, test_handout_lease_is_not_directly_destroyed) {
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  uint8_t wasCalled = 0;
  auto ignoreMe = [&wasCalled]() noexcept -> void {
    wasCalled++;
  };
  auto networkMock = getNetworkMock(leaseManager);
  // We are not allowed to abort remote leases here.
  EXPECT_CALL(*networkMock, abortIds(testing::_, testing::_)).Times(0);
  {
    auto leaseId = LeaseId{42};
    auto guardOne = leaseManager.handoutLease(leaseIsFor, leaseId, ignoreMe);
    ASSERT_TRUE(guardOne.ok()) << "Failed to handout a lease with given ID";
    EXPECT_EQ(guardOne->id(), leaseId) << "LeaseId should be the same.";
    waitForSchedulerEmpty();
    EXPECT_EQ(wasCalled, 0)
        << "The guard is still inside the result. Callback is not allowed to be called";

    // We now go out of scope. Can call abort now.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{leaseId})).Times(1);
  }
  EXPECT_EQ(wasCalled, 0)
      << "We have now locally lost the lease, should not call abort";
}

TEST_F(LeaseManagerTest, test_cannot_handout_same_lease_id_twice_for_same_peer) {
  auto leaseManager = buildManager();
  auto networkMock = getNetworkMock(leaseManager);
  auto leaseIsFor = getPeerState(serverA);
  uint8_t wasCalled = 0;
  auto ignoreMe = [&wasCalled]() noexcept -> void {
    wasCalled++;
  };
  // We are not allowed to abort remote leases here.
  EXPECT_CALL(*networkMock, abortIds(testing::_, testing::_)).Times(0);
  {
    auto leaseId = LeaseId{42};
    auto guardOne = leaseManager.handoutLease(leaseIsFor, leaseId, ignoreMe);
    ASSERT_TRUE(guardOne.ok()) << "Failed to handout a lease with given ID";
    EXPECT_EQ(guardOne->id(), leaseId) << "LeaseId should be the same.";
    auto guardTwo = leaseManager.handoutLease(leaseIsFor, leaseId, ignoreMe);
    EXPECT_FALSE(guardTwo.ok())
        << "Should not be able to handout the same lease ID twice.";
    EXPECT_EQ(wasCalled, 0)
        << "One of the abort callbacks triggered, should not happen.";

    // We now go out of scope. Can call abort now.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{leaseId})).Times(1);
  }
  EXPECT_EQ(wasCalled, 0)
      << "One of the abort callbacks triggered, should not happen, one fails "
         "to be created, the other goes out of scope";
}

TEST_F(LeaseManagerTest, test_a_lease_to_remote_can_be_moved_around) {
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  uint64_t wasCalled = 0;
  auto countingCallback = [&]() noexcept -> void {
    wasCalled++;
  };
  auto networkMock = getNetworkMock(leaseManager);
  // We are not allowed to abort remote leases here.
  EXPECT_CALL(*networkMock, abortIds(testing::_, testing::_)).Times(0);
  struct MyStructure {
    LeaseManager::LeaseToRemoteGuard lease;
  };
  {
    auto storedId = LeaseId{1337};
    auto guardOne = leaseManager.handoutLease(leaseIsFor, storedId, countingCallback);
    ASSERT_TRUE(guardOne.ok());
    auto myStructure = MyStructure{.lease = std::move(guardOne.get())};
    waitForSchedulerEmpty();
    EXPECT_EQ(wasCalled, 0) << "Callback was called while moving around.";
    // We now go out of scope with myStruct. Can call abort now.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{storedId})).Times(1);
  }
}

TEST_F(LeaseManagerTest, test_can_handout_same_lease_id_twice_for_different_peers) {
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  auto leaseIsForOther = getPeerState(serverB);
  auto ignoreMe = []() noexcept -> void {};
  auto leaseId = LeaseId{42};
  auto guardOne = leaseManager.handoutLease(leaseIsFor, leaseId, ignoreMe);
  auto guardTwo = leaseManager.handoutLease(leaseIsForOther, leaseId, ignoreMe);
}

TEST_F(LeaseManagerTest, test_lease_is_removed_from_list_on_guard_destruction) {
  bool rebootCallbackCalled = false;
  auto leaseManager = buildManager();
  auto networkMock = getNetworkMock(leaseManager);
  auto leaseIsFor = getPeerState(serverA);
  LeaseId storedId;
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.requireLease(leaseIsFor, callback);
    storedId = lease.id();
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                      lease.id());
    // Prepare to be called to abort this ID.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{storedId})).Times(1);
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            storedId);

  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  waitForSchedulerEmpty();
  // We locally lost the lease, we should not call the onLeaseLost callback.
  EXPECT_FALSE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_to_remote_is_removed_from_list_on_guard_destruction) {
  bool rebootCallbackCalled = false;
  auto leaseManager = buildManager();
  auto networkMock = getNetworkMock(leaseManager);
  auto leaseIsTo = getPeerState(serverA);
  LeaseId storedId{42};
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.handoutLease(leaseIsTo, storedId, callback);
    ASSERT_TRUE(lease.ok()) << "Failed to handout a lease with given ID: " << lease.errorMessage();


    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedToListContainsLease(leaseReport.slice(), leaseIsTo,
                                      lease->id());
    // Prepare to be called to abort this ID.
    EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{storedId})).Times(1);
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsTo,
                                            storedId);

  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  waitForSchedulerEmpty();
  // We locally lost the lease, we should not call the onLeaseLost callback.
  EXPECT_FALSE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_from_remote_can_cancel_abort_callback) {
  bool rebootCallbackCalled = false;
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  LeaseId storedId;
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.requireLease(leaseIsFor, callback);
    storedId = lease.id();
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                      lease.id());
    lease.cancel();
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            storedId);
  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  // (Should not do anything here)
  waitForSchedulerEmpty();
  // We locally lost the lease, we should not call the onLeaseLost callback.
  // So completing the lease does not actually change anything here.
  EXPECT_FALSE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_to_remote_can_cancel_abort_callback) {
  bool rebootCallbackCalled = false;
  auto leaseManager = buildManager();
  auto leaseIsFor = getPeerState(serverA);
  LeaseId storedId{42};
  {
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto lease = leaseManager.handoutLease(leaseIsFor, storedId, callback);
    ASSERT_TRUE(lease.ok()) << "Failed to handout a lease with given ID: " << lease.errorMessage();
    storedId = lease->id();
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedToListContainsLease(leaseReport.slice(), leaseIsFor,
                                    lease->id());
    lease->cancel();
  }
  {
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                          storedId);
  }
  // Need to wait for the scheduler to actually work on the RebootTracker.
  // (Should not do anything here)
  waitForSchedulerEmpty();
  // We locally lost the lease, we should not call the onLeaseLost callback.
  // So completing the lease does not actually change anything here.
  EXPECT_FALSE(rebootCallbackCalled);
}

TEST_F(LeaseManagerTest, test_lease_from_remote_is_aborted_on_peer_reboot) {
  {
    bool rebootCallbackCalled = false;
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    [[maybe_unused]] auto lease =
        leaseManager.requireLease(leaseIsFor, callback);
    rebootServer(serverA);
    // After a reboot of the other server, the onLeaseAbort callback should be
    // triggered
    EXPECT_TRUE(rebootCallbackCalled);
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            lease.id());
  }
}

TEST_F(LeaseManagerTest, test_lease_to_remote_is_aborted_on_peer_reboot) {
  {
    bool rebootCallbackCalled = false;
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    LeaseId storedId{42};
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto lease =
        leaseManager.handoutLease(leaseIsFor, storedId, callback);
    ASSERT_TRUE(lease.ok()) << "Failed to handout a lease with given ID: " << lease.errorMessage();
    rebootServer(serverA);
    // After a reboot of the other server, the onLeaseAbort callback should be
    // triggered
    EXPECT_TRUE(rebootCallbackCalled);
    auto leaseReport = leaseManager.leasesToVPack();
    assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            lease->id());
  }
}

TEST_F(LeaseManagerTest, test_canceled_lease_from_remote_is_not_aborted_on_peer_reboot) {
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    [[maybe_unused]] auto lease =
        leaseManager.requireLease(leaseIsFor, callback);
    lease.cancel();
    {
      // Cancel does not take the Lease out of the list!
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                        lease.id());
    }
    rebootServer(serverA);
    // NOTE: Lease is still in Scope, but the callback should not be called.
    {
      // Rebooting the server does remove the lease from the list.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                              lease.id());
    }
    EXPECT_FALSE(rebootCallbackCalled)
        << "Called callback on canceled lease if server rebooted";
  }
  EXPECT_FALSE(rebootCallbackCalled)
      << "Called callback on canceled lease if guard goes out of scope";
}

TEST_F(LeaseManagerTest,
       test_canceled_lease_to_remote_is_not_aborted_on_peer_reboot) {
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };
    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    LeaseId id{42};
    // We need to hold the lease until the end of the scope.
    // otherwise the destructor callback might be lost.
    auto lease =
        leaseManager.handoutLease(leaseIsFor, id, callback);
    ASSERT_TRUE(lease.ok()) << "Failed to handout a lease with given ID: " << lease.errorMessage();
    lease->cancel();
    {
      // Cancel does not take the Lease out of the list!
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListContainsLease(leaseReport.slice(), leaseIsFor,
                                      lease->id());
    }
    rebootServer(serverA);
    // NOTE: Lease is still in Scope, but the callback should not be called.
    {
      // Rebooting the server does remove the lease from the list.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            lease->id());
    }
    EXPECT_FALSE(rebootCallbackCalled)
        << "Called callback on canceled lease if server rebooted";
  }
  EXPECT_FALSE(rebootCallbackCalled)
      << "Called callback on canceled lease if guard goes out of scope";
}

TEST_F(LeaseManagerTest, test_acquire_lease_for_rebooted_server) {
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };

    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);

    rebootServer(serverA);
    // Now ServerA is rebooted and the peerState is outdated.
    ASSERT_LT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, server was not rebooted.";

    auto lease = leaseManager.requireLease(leaseIsFor, callback);

    // Requiring a least for an outdated peerState should actually trigger the
    // RebootTracker to intervene.
    waitForSchedulerEmpty();
    {
      // This situation is handled the same as if reboot would be AFTER
      // getting the lease. So Server should be dropped here.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                              lease.id());
    }
    EXPECT_TRUE(rebootCallbackCalled)
        << "We registered a lease for a dead server. We need to get called.";
  }
}

TEST_F(LeaseManagerTest, test_handout_lease_for_rebooted_server) {
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };

    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    LeaseId id{42};

    rebootServer(serverA);
    // Now ServerA is rebooted and the peerState is outdated.
    ASSERT_LT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, server was not rebooted.";

    auto lease = leaseManager.handoutLease(leaseIsFor, id, callback);

    // Requiring a least for an outdated peerState should actually trigger the
    // RebootTracker to intervene.
    waitForSchedulerEmpty();
    {
      // This situation is handled the same as if reboot would be AFTER
      // getting the lease. So Server should be dropped here.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            id);
    }
    EXPECT_TRUE(rebootCallbackCalled)
        << "We registered a lease for a dead server. We need to get called.";
  }
}

TEST_F(LeaseManagerTest, test_acquire_lease_for_server_with_newer_reboot_id) {
  // NOTE: This can happen in production in the following way:
  // 1. Server is rebooted, Local State is updated.
  // 2. RebootTracker schedules the Handling Callbacks.
  // 3. The caller now LooksUp the Local State and gets the new RebootId.
  // 4. The caller now tries to acquire a Lease for the new RebootId.
  // 5. Only now the handling Callbacks scheduled in 2. are executed.
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept {
      rebootCallbackCalled = true;
    };

    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    leaseIsFor.rebootId = RebootId{leaseIsFor.rebootId.value() + 1};

    // Now ServerA is rebooted, the RebootTracker has not yet handled it.
    ASSERT_GT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, lease is not ahead of RebootTracker.";

    auto lease = leaseManager.requireLease(leaseIsFor, callback);

    // Give RebootTracker time to intervene.
    waitForSchedulerEmpty();

    EXPECT_FALSE(rebootCallbackCalled) << "We are ahead of the RebootTracker. So we should not get aborted.";
    {
      // Lease should be in the Report:
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                        lease.id());
    }

    // Now move RebootTracker to new state, it now sees sme id as the leaser.
    rebootServer(serverA);

    ASSERT_EQ(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, RebootIds should now be aligned.";

    EXPECT_FALSE(rebootCallbackCalled) << "We are ahead of the RebootTracker. So we should not get aborted.";
    {
      // Lease should be in the Report:
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListContainsLease(leaseReport.slice(), leaseIsFor,
                                        lease.id());
    }

    // Reboot once more. Now we should be behind the RebootTracker. Causing the callback to be called.
    rebootServer(serverA);

    ASSERT_LT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, RebootId of Tracker should now be ahead of Lease.";

    EXPECT_TRUE(rebootCallbackCalled) << "Now the reboot tracker has overtaken us, we need to be aborted.";
    {
      // This situation is handled the same as if reboot would be AFTER
      // getting the lease. So Server should be dropped here.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedFromListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                              lease.id());
    }
  }
}

TEST_F(LeaseManagerTest, test_handout_lease_for_server_with_newer_reboot_id) {
  // NOTE: This can happen in production in the following way:
  // 1. A peer server reboots, and then sends out a require lease request
  // 2. The server running the manager receives the request, before the reboot
  // tracker has updated the state.
  // 3. Then it has to handout the lease, for a seemingly newer server version.
  bool rebootCallbackCalled = false;
  {
    auto callback = [&]() noexcept { rebootCallbackCalled = true; };

    auto leaseManager = buildManager();
    auto leaseIsFor = getPeerState(serverA);
    leaseIsFor.rebootId = RebootId{leaseIsFor.rebootId.value() + 1};

    // Now ServerA is rebooted, the RebootTracker has not yet handled it.
    ASSERT_GT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, lease is not ahead of RebootTracker.";

    LeaseId id{42};
    auto lease = leaseManager.handoutLease(leaseIsFor, id, callback);
    ASSERT_TRUE(lease.ok()) << "Failed to handout a lease with given ID: " << lease.errorMessage();

    // Give RebootTracker time to intervene.
    waitForSchedulerEmpty();

    EXPECT_FALSE(rebootCallbackCalled)
        << "We are ahead of the RebootTracker. So we should not get aborted.";
    {
      // Lease should be in the Report:
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListContainsLease(leaseReport.slice(), leaseIsFor,
                                      lease->id());
    }

    // Now move RebootTracker to new state, it now sees sme id as the leaser.
    rebootServer(serverA);

    ASSERT_EQ(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, RebootIds should now be aligned.";

    EXPECT_FALSE(rebootCallbackCalled)
        << "We are ahead of the RebootTracker. So we should not get aborted.";
    {
      // Lease should be in the Report:
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListContainsLease(leaseReport.slice(), leaseIsFor,
                                      lease->id());
    }

    // Reboot once more. Now we should be behind the RebootTracker. Causing the
    // callback to be called.
    rebootServer(serverA);

    ASSERT_LT(leaseIsFor.rebootId, state.find(serverA)->second.rebootId)
        << "Test setup incorrect, RebootId of Tracker should now be ahead of "
           "Lease.";

    EXPECT_TRUE(rebootCallbackCalled)
        << "Now the reboot tracker has overtaken us, we need to be aborted.";
    {
      // This situation is handled the same as if reboot would be AFTER
      // getting the lease. So Server should be dropped here.
      auto leaseReport = leaseManager.leasesToVPack();
      assertLeasedToListDoesNotContainLease(leaseReport.slice(), leaseIsFor,
                                            lease->id());
    }
  }
}

// TODO Add tests with Multiple Servers.

#if 0
// Example imeplementation to create a bad response on the networkMock.
EXPECT_CALL(*networkMock, abortIds(serverA, std::vector<LeaseId>{storedId})).WillOnce([&]() -> auto {
  auto promise = futures::Promise<Result>{};
  auto future = promise.getFuture();
  scheduler->queue(RequestLane::CONTINUATION, [promise = std::move(promise)]() mutable -> void {
    promise.setValue(Result{TRI_ERROR_HTTP_NOT_FOUND});
  });
  return future;
});
#endif