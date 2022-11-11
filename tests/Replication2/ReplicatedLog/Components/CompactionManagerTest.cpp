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

#include "Replication2/ReplicatedLog/Components/CompactionManager.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"

#include <Futures/Future.h>
#include <Futures/Promise.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {

struct StorageTransactionMock : IStorageTransaction {
  MOCK_METHOD(InMemoryLog, getInMemoryLog, (), (const, noexcept, override));
  MOCK_METHOD(LogRange, getLogBounds, (), (const, noexcept, override));
  MOCK_METHOD(futures::Future<Result>, removeFront, (LogIndex),
              (noexcept, override));
  MOCK_METHOD(futures::Future<Result>, appendEntries,
              (LogIndex, InMemoryLogSlice), (noexcept, override));
};

struct StorageManagerMock : IStorageManager {
  MOCK_METHOD(std::unique_ptr<IStorageTransaction>, transaction, (),
              (override));
};

struct SchedulerInterfaceMock : ISchedulerInterface {};

}  // namespace

struct CompactionManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storageManagerMock;
  SchedulerInterfaceMock scheduler;

  std::shared_ptr<ReplicatedLogGlobalSettings> options =
      std::make_shared<ReplicatedLogGlobalSettings>();

  std::shared_ptr<CompactionManager> compactionManager =
      std::make_shared<CompactionManager>(storageManagerMock, scheduler,
                                          options);
};

TEST_F(CompactionManagerTest, no_compaction_after_release_index_update) {
  LogRange const range{LogIndex{0}, LogIndex{101}};
  options->_thresholdLogCompaction = 0;

  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds).WillByDefault(testing::Return(range));
    EXPECT_CALL(*trx, getLogBounds).Times(1).WillOnce(testing::Return(range));
    return trx;
  });

  EXPECT_CALL(storageManagerMock, transaction).Times(1);

  compactionManager->updateReleaseIndex(LogIndex{20});

  auto const status = compactionManager->getCompactionStatus();
  ASSERT_NE(status.stop, std::nullopt);
  ASSERT_TRUE(
      std::holds_alternative<CompactionStopReason::LeaderBlocksReleaseEntry>(
          status.stop->value))
      << "actual index: " << status.stop->value.index();
}

TEST_F(CompactionManagerTest,
       no_compaction_after_largest_index_to_keep_update) {
  LogRange const range{LogIndex{0}, LogIndex{101}};
  options->_thresholdLogCompaction = 0;

  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds).WillByDefault(testing::Return(range));
    EXPECT_CALL(*trx, getLogBounds).Times(1).WillOnce(testing::Return(range));
    return trx;
  });

  EXPECT_CALL(storageManagerMock, transaction).Times(1);

  compactionManager->updateLargestIndexToKeep(LogIndex{20});

  auto const status = compactionManager->getCompactionStatus();
  ASSERT_NE(status.stop, std::nullopt);
  ASSERT_TRUE(
      std::holds_alternative<CompactionStopReason::NotReleasedByStateMachine>(
          status.stop->value))
      << "actual index: " << status.stop->value.index();
  auto const& detail =
      std::get<CompactionStopReason::NotReleasedByStateMachine>(
          status.stop->value);
  EXPECT_EQ(detail.releasedIndex, LogIndex{0});
}

TEST_F(CompactionManagerTest, no_compaction_because_of_threshold) {
  LogRange const range{LogIndex{0}, LogIndex{101}};
  options->_thresholdLogCompaction = 50;

  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds).WillByDefault(testing::Return(range));
    EXPECT_CALL(*trx, getLogBounds).Times(1).WillOnce(testing::Return(range));
    return trx;
  });

  EXPECT_CALL(storageManagerMock, transaction).Times(2);

  compactionManager->updateLargestIndexToKeep(LogIndex{20});
  compactionManager->updateReleaseIndex(LogIndex{45});

  auto const status = compactionManager->getCompactionStatus();
  ASSERT_NE(status.stop, std::nullopt);
  ASSERT_TRUE(std::holds_alternative<
              CompactionStopReason::CompactionThresholdNotReached>(
      status.stop->value))
      << "actual index: " << status.stop->value.index();
  auto const& detail =
      std::get<CompactionStopReason::CompactionThresholdNotReached>(
          status.stop->value);
  EXPECT_EQ(detail.nextCompactionAt, LogIndex{50});
}

TEST_F(CompactionManagerTest, run_automatic_compaction) {
  LogRange const range{LogIndex{0}, LogIndex{101}};
  options->_thresholdLogCompaction = 0;

  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds).WillByDefault(testing::Return(range));
    EXPECT_CALL(*trx, getLogBounds).Times(1).WillOnce(testing::Return(range));
    return trx;
  });

  EXPECT_CALL(storageManagerMock, transaction).Times(2);

  compactionManager->updateLargestIndexToKeep(LogIndex{20});

  {
    auto const status = compactionManager->getCompactionStatus();
    ASSERT_EQ(status.inProgress, std::nullopt);
  }

  std::optional<futures::Promise<Result>> p;
  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds).WillByDefault(testing::Return(range));
    EXPECT_CALL(*trx, getLogBounds).Times(1).WillOnce(testing::Return(range));

    ON_CALL(*trx, removeFront).WillByDefault([&](LogIndex stop) {
      return p.emplace().getFuture();
    });
    EXPECT_CALL(*trx, removeFront).Times(1);
    return trx;
  });

  compactionManager->updateReleaseIndex(
      LogIndex{45});  // should start a compaction now

  {
    auto const status = compactionManager->getCompactionStatus();
    ASSERT_NE(status.inProgress, std::nullopt);
    // compaction is possible upto entry 20
    EXPECT_EQ(status.inProgress->range, (LogRange{LogIndex{0}, LogIndex{20}}));
  }

  EXPECT_CALL(storageManagerMock, transaction).Times(1);
  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });

  // now resolve the promise
  p->setValue(TRI_ERROR_NO_ERROR);

  {
    auto const status = compactionManager->getCompactionStatus();
    ASSERT_NE(status.lastCompaction, std::nullopt);
    // old compaction stored
    EXPECT_EQ(status.lastCompaction->range,
              (LogRange{LogIndex{0}, LogIndex{20}}));
    // now other compaction going
    EXPECT_EQ(status.inProgress, std::nullopt);
  }
}
