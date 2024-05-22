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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/Components/StateHandleManager.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

#include "Replication2/Mocks/ReplicatedStateHandleMock.h"
#include "Replication2/Mocks/FollowerCommitManagerMock.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct StateHandleManagerTest : testing::Test {};

TEST_F(StateHandleManagerTest, resign) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto originalPtr = stateHandlePtr.get();

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  // Times is good enough here, the return value will be ignored anyway.
  EXPECT_CALL(*originalPtr, resignCurrentState).Times(1);
  auto stateHandle = mgr.resign();
  EXPECT_EQ(originalPtr, stateHandle.get())
      << "We expect the initial stateHandle to be returned when we resign";
}

TEST_F(StateHandleManagerTest, acquireSnaphot) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};
  ParticipantId leaderId{"leader"};
  uint64_t version{42};

  // Times is good enough here, we are a void method.
  // Important part is that the input values are forwarded correctly.
  EXPECT_CALL(*mockPtr, acquireSnapshot(leaderId, LogIndex{0}, version))
      .Times(1);

  mgr.acquireSnapshot(leaderId, version);
}

TEST_F(StateHandleManagerTest, acquireSnaphot_after_resign) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};
  ParticipantId leaderId{"leader"};
  uint64_t version{42};

  // We call resign once
  EXPECT_CALL(*mockPtr, resignCurrentState).Times(1);
  // We cannot call acquireSnapshot after resigning.
  EXPECT_CALL(*mockPtr, acquireSnapshot).Times(0);

  auto stateHandle = mgr.resign();
  ASSERT_TRUE(stateHandle != nullptr);
  mgr.acquireSnapshot(leaderId, version);
}

TEST_F(StateHandleManagerTest, becomeFollower) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  auto methods = std::unique_ptr<IReplicatedLogFollowerMethods>{};
  auto methodsPtr = methods.get();

  // Times is good enough here, we are a void method.
  // Important part is that the input values are forwarded correctly.
  EXPECT_CALL(*mockPtr, becomeFollower)
      .WillOnce([&](std::unique_ptr<IReplicatedLogFollowerMethods> methods)
                    -> void {
        EXPECT_EQ(methodsPtr, methods.get())
            << "Did not call the mocked method with the correct methods object";
      });

  mgr.becomeFollower(std::move(methods));
}

TEST_F(StateHandleManagerTest, updateCommitIndex_no_resolveIndex) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();
  bool deferredActionCalled = false;

  LogIndex expectedIndex{42};
  bool expectedSnapshotAvailable = true;

  EXPECT_CALL(followerCommitManager, updateCommitIndex)
      .WillOnce([&](LogIndex index, bool snapshotAvailable) {
        EXPECT_EQ(expectedIndex, index);
        EXPECT_EQ(expectedSnapshotAvailable, snapshotAvailable);
        DeferredAction actionDummy{
            [&]() noexcept { deferredActionCalled = true; }};
        return std::make_pair<std::optional<LogIndex>, DeferredAction>(
            std::nullopt, std::move(actionDummy));
      });
  // As we do not return a LogIndex above we are not allowed to call
  // updateCommitIndex on the Replicated State Handle.
  EXPECT_CALL(*mockPtr, updateCommitIndex).Times(0);

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  auto action = mgr.updateCommitIndex(expectedIndex, expectedSnapshotAvailable);
  EXPECT_TRUE(action) << "We expect to have an action";
  EXPECT_FALSE(deferredActionCalled)
      << "Already called action, it was not deferred";
  action.fire();
  EXPECT_TRUE(deferredActionCalled)
      << "Deferred different action then expected";
}

TEST_F(StateHandleManagerTest, updateCommitIndex_with_resolveIndex) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();
  bool deferredActionCalled = false;

  LogIndex expectedIndex{42};
  bool expectedSnapshotAvailable = true;

  EXPECT_CALL(followerCommitManager, updateCommitIndex)
      .WillOnce([&](LogIndex index, bool snapshotAvailable) {
        EXPECT_EQ(expectedIndex, index);
        EXPECT_EQ(expectedSnapshotAvailable, snapshotAvailable);
        DeferredAction actionDummy{
            [&]() noexcept { deferredActionCalled = true; }};
        return std::make_pair<std::optional<LogIndex>, DeferredAction>(
            LogIndex{23}, std::move(actionDummy));
      });
  EXPECT_CALL(*mockPtr, updateCommitIndex).WillOnce([](LogIndex index) {
    EXPECT_EQ(index, LogIndex{23})
        << "Was not called with update commit index result";
  });

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  auto action = mgr.updateCommitIndex(expectedIndex, expectedSnapshotAvailable);
  EXPECT_TRUE(action) << "We expect to have an action";
  EXPECT_FALSE(deferredActionCalled)
      << "Already called action, it was not deferred";
  action.fire();
  EXPECT_TRUE(deferredActionCalled)
      << "Deferred different action then expected";
}

TEST_F(StateHandleManagerTest, updateCommitIndex_after_resign) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  auto mockPtr = stateHandlePtr.get();
  LogIndex expectedIndex{42};
  bool expectedSnapshotAvailable = true;

  EXPECT_CALL(*mockPtr, resignCurrentState).Times(1);
  // We are not allowed to call any updateCommitIndex after we resign
  EXPECT_CALL(followerCommitManager, updateCommitIndex).Times(0);
  EXPECT_CALL(*mockPtr, updateCommitIndex).Times(0);

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  auto stateHandle = mgr.resign();
  ASSERT_TRUE(stateHandle != nullptr);

  auto action = mgr.updateCommitIndex(expectedIndex, expectedSnapshotAvailable);
  // Action has to be empty and cannot do any harm, lets fire it.
  EXPECT_FALSE(action) << "We actually got an action back, that is not allowed";
  action.fire();
}

TEST_F(StateHandleManagerTest, get_internal_status) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  // We need to setup all expected calls for the stateHandlePtr here.
  // we will move it out of our scope.
  EXPECT_CALL(*stateHandlePtr, getInternalStatus)
      .WillOnce([]() -> replicated_state::Status {
        return {replicated_state::Status::Follower{
            replicated_state::Status::Follower::Constructed{LogIndex{0}}}};
      });

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};

  auto status = mgr.getInternalStatus();
  ASSERT_TRUE(
      std::holds_alternative<replicated_state::Status::Follower>(status.value));
  auto followerStatus =
      std::get<replicated_state::Status::Follower>(status.value);
  ASSERT_TRUE(
      std::holds_alternative<replicated_state::Status::Follower::Constructed>(
          followerStatus.value));
  auto constructedFollowerStatus =
      std::get<replicated_state::Status::Follower::Constructed>(
          followerStatus.value);
  EXPECT_EQ(constructedFollowerStatus.appliedIndex, LogIndex{0});
}

TEST_F(StateHandleManagerTest, get_internal_status_after_resign) {
  auto stateHandlePtr = std::make_unique<test::ReplicatedStateHandleMock>();
  test::FollowerCommitManagerMock followerCommitManager{};
  // We need to setup all expected calls for the stateHandlePtr here.
  // we will move it out of our scope.
  // We are not allowed to call this method after resigning.
  // We are asked to resign.
  EXPECT_CALL(*stateHandlePtr, resignCurrentState).Times(1);
  // Then we are asked for InternalStatus, that is not allowed.
  EXPECT_CALL(*stateHandlePtr, getInternalStatus).Times(0);

  StateHandleManager mgr{std::move(stateHandlePtr), followerCommitManager};
  auto stateHandle = mgr.resign();
  ASSERT_TRUE(stateHandle != nullptr);

  auto status = mgr.getInternalStatus();
  ASSERT_TRUE(
      std::holds_alternative<replicated_state::Status::Follower>(status.value));
  auto followerStatus =
      std::get<replicated_state::Status::Follower>(status.value);
  ASSERT_TRUE(
      std::holds_alternative<replicated_state::Status::Follower::Resigned>(
          followerStatus.value));
}