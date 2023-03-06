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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/Components/SnapshotManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedLog/Components/TermInformation.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

namespace {

struct StorageManagerMock;

struct StorageTransactionMock : IStorageTransaction {
  MOCK_METHOD(InMemoryLog, getInMemoryLog, (), (const, noexcept, override));
  MOCK_METHOD(LogRange, getLogBounds, (), (const, noexcept, override));
  MOCK_METHOD(futures::Future<Result>, removeFront, (LogIndex),
              (noexcept, override));
  MOCK_METHOD(futures::Future<Result>, removeBack, (LogIndex),
              (noexcept, override));
  MOCK_METHOD(futures::Future<Result>, appendEntries, (InMemoryLog),
              (noexcept, override));
};

struct StateInfoTransactionMock : IStateInfoTransaction {
  MOCK_METHOD(InfoType&, get, (), (noexcept, override));
};

struct StorageManagerMock : IStorageManager {
  MOCK_METHOD(std::unique_ptr<IStorageTransaction>, transaction, (),
              (override));
  MOCK_METHOD(InMemoryLog, getCommittedLog, (), (const, override));
  MOCK_METHOD(std::unique_ptr<TypedLogRangeIterator<LogEntryView>>,
              getCommittedLogIterator, (LogRange), (const, override));
  MOCK_METHOD(TermIndexMapping, getTermIndexMapping, (), (const, override));
  MOCK_METHOD(replicated_state::PersistedStateInfo, getCommittedMetaInfo, (),
              (const, override));
  MOCK_METHOD(std::unique_ptr<IStateInfoTransaction>, beginMetaInfoTrx, (),
              (override));
  MOCK_METHOD(Result, commitMetaInfoTrx,
              (std::unique_ptr<IStateInfoTransaction>), (override));
};

struct StateHandleManagerMock : IStateHandleManager {
  MOCK_METHOD(void, updateCommitIndex, (LogIndex), (noexcept, override));
  MOCK_METHOD(void, becomeFollower,
              (std::unique_ptr<IReplicatedLogFollowerMethods>),
              (noexcept, override));
  MOCK_METHOD(std::unique_ptr<IReplicatedStateHandle>, resign, (),
              (noexcept, override));
  MOCK_METHOD(void, acquireSnapshot, (ParticipantId const&, std::uint64_t),
              (noexcept, override));
};

struct LeaderCommunicatorMock : ILeaderCommunicator {
  MOCK_METHOD(ParticipantId const&, getParticipantId, (),
              (const, noexcept, override));
  MOCK_METHOD(futures::Future<Result>, reportSnapshotAvailable, (MessageId mid),
              (noexcept, override));
};
}  // namespace

struct SnapshotManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storageManagerMock;
  testing::StrictMock<StateHandleManagerMock> stateHandleManagerMock;

  std::shared_ptr<LeaderCommunicatorMock> leaderComm =
      std::make_shared<LeaderCommunicatorMock>();
  std::shared_ptr<FollowerTermInformation> termInfo =
      std::make_shared<FollowerTermInformation>(
          FollowerTermInformation{.leader = "LEADER"});

  auto constructSnapshotManager() {
    return std::make_shared<SnapshotManager>(
        storageManagerMock, stateHandleManagerMock, termInfo, leaderComm,
        LoggerContext{Logger::REPLICATION2});
  }
};

TEST_F(SnapshotManagerTest, create_with_valid_snapshot) {
  EXPECT_CALL(storageManagerMock, getCommittedMetaInfo).WillOnce([] {
    return replicated_state::PersistedStateInfo{
        .snapshot = {.status = SnapshotStatus::kCompleted}};
  });

  auto snapMan = constructSnapshotManager();
}

TEST_F(SnapshotManagerTest, create_with_invalid_snapshot) {
  auto state = replicated_state::PersistedStateInfo{
      .snapshot = {.status = SnapshotStatus::kInvalidated}};
  EXPECT_CALL(storageManagerMock, getCommittedMetaInfo).WillOnce([&] {
    return state;
  });

  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 1));
  auto snapMan = constructSnapshotManager();
}

TEST_F(SnapshotManagerTest, invalidate_snapshot) {
  auto state = replicated_state::PersistedStateInfo{
      .snapshot = {.status = SnapshotStatus::kCompleted}};

  EXPECT_CALL(storageManagerMock, getCommittedMetaInfo).WillOnce([&] {
    return state;
  });

  auto snapMan = constructSnapshotManager();

  EXPECT_CALL(storageManagerMock, beginMetaInfoTrx).WillOnce([&] {
    auto trx = std::make_unique<testing::NiceMock<StateInfoTransactionMock>>();
    ON_CALL(*trx, get).WillByDefault([&]() -> auto& { return state; });
    return trx;
  });
  EXPECT_CALL(storageManagerMock, commitMetaInfoTrx);

  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 1));
  snapMan->invalidateSnapshotState();

  EXPECT_CALL(storageManagerMock, beginMetaInfoTrx).WillOnce([&] {
    auto trx = std::make_unique<testing::NiceMock<StateInfoTransactionMock>>();
    ON_CALL(*trx, get).WillByDefault([&]() -> auto& { return state; });
    return trx;
  });
  EXPECT_CALL(storageManagerMock, commitMetaInfoTrx).WillOnce([&](auto trx) {
    EXPECT_EQ(state.snapshot.status, SnapshotStatus::kCompleted);
    return Result{};
  });
  futures::Promise<Result> p;
  EXPECT_CALL(*leaderComm, reportSnapshotAvailable(MessageId{12}))
      .WillOnce([&](MessageId) { return p.getFuture(); });
  snapMan->setSnapshotStateAvailable(MessageId{12}, 1);
  p.setValue(Result{});
}

TEST_F(SnapshotManagerTest, invalidate_snapshot_twice) {
  auto state = replicated_state::PersistedStateInfo{
      .snapshot = {.status = SnapshotStatus::kCompleted}};

  EXPECT_CALL(storageManagerMock, getCommittedMetaInfo).WillOnce([&] {
    return state;
  });

  auto snapMan = constructSnapshotManager();
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::AVAILABLE);
  // TODO check calls in that order?
  EXPECT_CALL(storageManagerMock, beginMetaInfoTrx).WillOnce([&] {
    auto trx = std::make_unique<testing::NiceMock<StateInfoTransactionMock>>();
    ON_CALL(*trx, get).WillByDefault(
        [&]() -> replicated_state::PersistedStateInfo& { return state; });
    return trx;
  });
  EXPECT_CALL(storageManagerMock, commitMetaInfoTrx).WillOnce([&](auto trx) {
    EXPECT_EQ(state.snapshot.status, SnapshotStatus::kInvalidated);
    return Result{};
  });

  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 1));
  snapMan->invalidateSnapshotState();
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::MISSING);

  testing::Mock::VerifyAndClearExpectations(&storageManagerMock);

  // if called again, we will get 2
  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 2));
  snapMan->invalidateSnapshotState();

  snapMan->setSnapshotStateAvailable(MessageId{12}, 1);
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::MISSING);

  EXPECT_CALL(storageManagerMock, beginMetaInfoTrx).WillOnce([&] {
    auto trx = std::make_unique<testing::NiceMock<StateInfoTransactionMock>>();
    ON_CALL(*trx, get).WillByDefault([&]() -> auto& { return state; });
    return trx;
  });
  EXPECT_CALL(storageManagerMock, commitMetaInfoTrx).WillOnce([&](auto trx) {
    EXPECT_EQ(state.snapshot.status, SnapshotStatus::kCompleted);
    return Result{};
  });

  futures::Promise<Result> p;
  EXPECT_CALL(*leaderComm, reportSnapshotAvailable(MessageId{12}))
      .WillOnce([&](MessageId) { return p.getFuture(); });
  snapMan->setSnapshotStateAvailable(MessageId{12}, 2);
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::AVAILABLE);

  p.setValue(Result{});
}
