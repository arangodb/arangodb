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
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedLog/Components/TermInformation.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/Storage/PersistedStateInfo.h"

#include "Replication2/Mocks/LeaderCommunicatorMock.h"
#include "Replication2/Mocks/StateHandleManagerMock.h"
#include "Replication2/Mocks/StorageManagerMock.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

namespace {

struct StorageTransactionMock : IStorageTransaction {
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

}  // namespace

struct SnapshotManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storageManagerMock;
  testing::StrictMock<StateHandleManagerMock> stateHandleManagerMock;

  std::shared_ptr<tests::LeaderCommunicatorMock> leaderComm =
      std::make_shared<tests::LeaderCommunicatorMock>();
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
    return storage::PersistedStateInfo{
        .snapshot = {.status = SnapshotStatus::kCompleted}};
  });

  auto snapMan = constructSnapshotManager();
}

TEST_F(SnapshotManagerTest, create_with_invalid_snapshot) {
  auto state = storage::PersistedStateInfo{
      .snapshot = {.status = SnapshotStatus::kInvalidated}};
  EXPECT_CALL(storageManagerMock, getCommittedMetaInfo).WillOnce([&] {
    return state;
  });

  auto snapMan = constructSnapshotManager();
  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 1));
  snapMan->acquireSnapshotIfNecessary();
}

TEST_F(SnapshotManagerTest, invalidate_snapshot) {
  auto state = storage::PersistedStateInfo{
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
  auto snapshotInvalidated = snapMan->invalidateSnapshotState();
  EXPECT_EQ(snapshotInvalidated, Result());

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
  auto result = snapMan->setSnapshotStateAvailable(MessageId{12}, 1);
  EXPECT_EQ(result, Result());
  p.setValue(Result{});
}

TEST_F(SnapshotManagerTest, invalidate_snapshot_twice) {
  auto state = storage::PersistedStateInfo{
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
        [&]() -> storage::PersistedStateInfo& { return state; });
    return trx;
  });
  EXPECT_CALL(storageManagerMock, commitMetaInfoTrx).WillOnce([&](auto trx) {
    EXPECT_EQ(state.snapshot.status, SnapshotStatus::kInvalidated);
    return Result{};
  });

  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 1));
  auto snapshotInvalidated = snapMan->invalidateSnapshotState();
  EXPECT_EQ(snapshotInvalidated, Result());
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::MISSING);

  testing::Mock::VerifyAndClearExpectations(&storageManagerMock);

  // if called again, we will get 2
  EXPECT_CALL(stateHandleManagerMock, acquireSnapshot("LEADER", 2));
  snapshotInvalidated = snapMan->invalidateSnapshotState();
  EXPECT_EQ(snapshotInvalidated, Result());

  auto result = snapMan->setSnapshotStateAvailable(MessageId{12}, 1);
  EXPECT_EQ(result, Result());
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
  result = snapMan->setSnapshotStateAvailable(MessageId{12}, 2);
  EXPECT_EQ(result, Result());
  EXPECT_EQ(snapMan->checkSnapshotState(), SnapshotState::AVAILABLE);

  p.setValue(Result{});
}
