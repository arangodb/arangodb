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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Replication2/ReplicatedLog/Components/CompactionManager.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/Mocks/StorageManagerMock.h"
#include "Replication2/Mocks/StorageTransactionMock.h"

#include <Futures/Future.h>
#include <Futures/Promise.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;
using namespace arangodb::replication2::replicated_log;

struct CompactionManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storageManagerMock;

  std::shared_ptr<ReplicatedLogGlobalSettings> options =
      std::make_shared<ReplicatedLogGlobalSettings>();

  std::shared_ptr<CompactionManager> compactionManager =
      std::make_shared<CompactionManager>(storageManagerMock, options,
                                          LoggerContext{Logger::REPLICATION2});
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

  compactionManager->updateLowestIndexToKeep(LogIndex{20});

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

  compactionManager->updateLowestIndexToKeep(LogIndex{20});
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
  options->_thresholdLogCompaction = 0;

  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{0}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });

  EXPECT_CALL(storageManagerMock, transaction).Times(2);

  compactionManager->updateLowestIndexToKeep(LogIndex{20});

  {
    auto const status = compactionManager->getCompactionStatus();
    ASSERT_EQ(status.inProgress, std::nullopt);
  }

  std::optional<futures::Promise<Result>> p;
  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{0}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);

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

  // now resolve the removeFront-promise
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

TEST_F(CompactionManagerTest, manual_compaction_call_nothing_to_compact_ok) {
  EXPECT_CALL(storageManagerMock, transaction).Times(1);
  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });

  auto cf = compactionManager->compact();
  ASSERT_TRUE(cf.isReady());
  auto result = cf.waitAndGet();
  EXPECT_EQ(result.error, std::nullopt);
  EXPECT_TRUE(result.compactedRange.empty());
}

TEST_F(CompactionManagerTest, manual_compaction_call_ok) {
  EXPECT_CALL(storageManagerMock, transaction).Times(2);
  ON_CALL(storageManagerMock, transaction).WillByDefault([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });

  // compaction possible upto 40, but threshold blocks
  options->_thresholdLogCompaction = 100;
  compactionManager->updateReleaseIndex(LogIndex{40});
  compactionManager->updateLowestIndexToKeep(LogIndex{40});

  {
    auto status = compactionManager->getCompactionStatus();
    ASSERT_NE(std::nullopt, status.stop);
    EXPECT_TRUE(std::holds_alternative<
                CompactionStopReason::CompactionThresholdNotReached>(
        status.stop->value));
  }

  std::optional<futures::Promise<Result>> removeFrontPromise;

  EXPECT_CALL(storageManagerMock, transaction)
      .Times(2)
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .Times(1)
            .WillOnce(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
        EXPECT_CALL(*trx, removeFront(LogIndex{40}))
            .Times(1)
            .WillOnce([&](LogIndex stop) {
              return removeFrontPromise.emplace().getFuture();
            });
        return trx;
      })
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .Times(1)
            .WillOnce(testing::Return(LogRange{LogIndex{40}, LogIndex{101}}));
        return trx;
      });

  auto cf = compactionManager->compact();
  ASSERT_FALSE(cf.isReady());
  // promise should be set now
  ASSERT_NE(removeFrontPromise, std::nullopt);
  removeFrontPromise->setValue(Result{});

  ASSERT_TRUE(cf.isReady());
  auto result = cf.waitAndGet();
  EXPECT_EQ(result.error, std::nullopt);
  EXPECT_EQ(result.compactedRange, (LogRange{LogIndex{20}, LogIndex{40}}));
}

TEST_F(CompactionManagerTest, run_automatic_compaction_twice) {
  options->_thresholdLogCompaction = 0;

  EXPECT_CALL(storageManagerMock, transaction).WillOnce([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{0}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });
  compactionManager->updateLowestIndexToKeep(LogIndex{20});

  // We expect two transactions to be started:
  // 1. see the log in range [1, 101) and calls removeFront with stop = 20
  // 2. sees the log in range [20, 101) and does nothing else (stops compaction
  // loop)
  EXPECT_CALL(storageManagerMock, transaction)
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{1}, LogIndex{101}}));
        EXPECT_CALL(*trx, removeFront(LogIndex{20}))
            .WillOnce([&](LogIndex stop) { return Result{}; });
        return trx;
      })
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
        return trx;
      });

  compactionManager->updateReleaseIndex(
      LogIndex{45});  // should start a compaction now

  // We expect two transactions to be started:
  // 1. see the log in range [20, 101) and calls removeFront with stop = 45
  // 2. sees the log in range [45, 101) and does nothing else (stops compaction
  // loop)
  EXPECT_CALL(storageManagerMock, transaction)
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
        EXPECT_CALL(*trx, removeFront(LogIndex{45}))
            .WillOnce([&](LogIndex stop) { return Result{}; });
        return trx;
      })
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{45}, LogIndex{101}}));
        return trx;
      });

  compactionManager->updateLowestIndexToKeep(
      LogIndex{45});  // should start another compaction now
}

TEST_F(CompactionManagerTest, run_automatic_compaction_twice_but_delayed) {
  options->_thresholdLogCompaction = 0;

  EXPECT_CALL(storageManagerMock, transaction).WillOnce([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    ON_CALL(*trx, getLogBounds)
        .WillByDefault(testing::Return(LogRange{LogIndex{0}, LogIndex{101}}));
    EXPECT_CALL(*trx, getLogBounds).Times(1);
    return trx;
  });
  compactionManager->updateLowestIndexToKeep(LogIndex{20});

  // We expect two transactions to be started:
  // 1. see the log in range [1, 101) and calls removeFront with stop = 20
  // This time however, we return a future from removeFront and resolve it
  // at the end.
  std::optional<futures::Promise<Result>> p;
  EXPECT_CALL(storageManagerMock, transaction).WillOnce([&] {
    auto trx = std::make_unique<testing::StrictMock<StorageTransactionMock>>();
    EXPECT_CALL(*trx, getLogBounds)
        .WillOnce(testing::Return(LogRange{LogIndex{1}, LogIndex{101}}));
    EXPECT_CALL(*trx, removeFront(LogIndex{20})).WillOnce([&](LogIndex stop) {
      return p.emplace().getFuture();
    });
    return trx;
  });

  compactionManager->updateReleaseIndex(
      LogIndex{45});  // should start a compaction now

  // We expect two transactions to be started:
  // 1. see the log in range [20, 101) and calls removeFront with stop = 45
  // 2. sees the log in range [45, 101) and does nothing else (stops compaction
  // loop)
  EXPECT_CALL(storageManagerMock, transaction)
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{20}, LogIndex{101}}));
        EXPECT_CALL(*trx, removeFront(LogIndex{45}))
            .WillOnce([&](LogIndex stop) { return Result{}; });
        return trx;
      })
      .WillOnce([&] {
        auto trx =
            std::make_unique<testing::StrictMock<StorageTransactionMock>>();
        EXPECT_CALL(*trx, getLogBounds)
            .WillOnce(testing::Return(LogRange{LogIndex{45}, LogIndex{101}}));
        return trx;
      });

  compactionManager->updateLowestIndexToKeep(
      LogIndex{45});  // should start another compaction now

  // finally resolve the future
  p->setValue(TRI_ERROR_NO_ERROR);
}

namespace {
auto operator""_Lx(unsigned long long idx) -> LogIndex { return LogIndex{idx}; }
}  // namespace

TEST(ComputeCompactionIndex, nothing_to_compact) {
  auto [index, reason] = CompactionManager::calculateCompactionIndex(
      12_Lx, 10_Lx, {1_Lx, 25_Lx}, 100);
  EXPECT_EQ(index, 1_Lx);
  EXPECT_TRUE(
      std::holds_alternative<
          CompactionStopReason::CompactionThresholdNotReached>(reason.value));
}

TEST(ComputeCompactionIndex, compact_upto_release_index) {
  auto [index, reason] = CompactionManager::calculateCompactionIndex(
      12_Lx, 15_Lx, {1_Lx, 25_Lx}, 10);
  EXPECT_EQ(index, 12_Lx);
  EXPECT_TRUE(
      std::holds_alternative<CompactionStopReason::NotReleasedByStateMachine>(
          reason.value));
}

TEST(ComputeCompactionIndex, compact_upto_largest_index_to_keep) {
  auto [index, reason] = CompactionManager::calculateCompactionIndex(
      13_Lx, 12_Lx, {1_Lx, 25_Lx}, 10);
  EXPECT_EQ(index, 12_Lx);
  EXPECT_TRUE(
      std::holds_alternative<CompactionStopReason::LeaderBlocksReleaseEntry>(
          reason.value));
}
