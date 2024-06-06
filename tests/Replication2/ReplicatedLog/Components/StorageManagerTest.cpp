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
#include <immer/flex_vector_transient.hpp>
#include <memory>

#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/InMemoryLogEntry.h"
#include "Replication2/ReplicatedLog/PersistedLogEntry.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/Storage/IteratorPosition.h"
#include "Replication2/Storage/PersistedStateInfo.h"

#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/StorageEngineMethodsMock.h"
#include "Replication2/Mocks/SchedulerMocks.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;
using replication2::storage::IStorageEngineMethods;

struct StorageManagerTest : ::testing::Test {
  std::uint64_t const objectId = 1;
  LogId const logId = LogId{12};
  std::shared_ptr<storage::rocksdb::test::DelayedExecutor> executor =
      std::make_shared<storage::rocksdb::test::DelayedExecutor>();
  std::shared_ptr<test::SyncScheduler> scheduler =
      std::make_shared<test::SyncScheduler>();
  storage::test::FakeStorageEngineMethodsContext methods{
      objectId, logId, executor, LogRange{LogIndex{1}, LogIndex{100}},
      storage::PersistedStateInfo{
          .stateId = logId,
          .snapshot = {.status = SnapshotStatus::kFailed,
                       .timestamp = {},
                       .error = {}},
          .generation = {},
          .specification = {}}};
  std::shared_ptr<StorageManager> storageManager =
      std::make_shared<StorageManager>(
          methods.getMethods(), LoggerContext{Logger::REPLICATION2}, scheduler);
};

TEST_F(StorageManagerTest, transaction_resign) {
  auto trx = storageManager->transaction();
  trx.reset();
  auto methods = storageManager->resign();
}

TEST_F(StorageManagerTest, transaction_resign_transaction) {
  auto trx = storageManager->transaction();
  trx.reset();
  auto methods = storageManager->resign();
  EXPECT_ANY_THROW({ std::ignore = storageManager->transaction(); });
}

TEST_F(StorageManagerTest, transaction_remove_front) {
  auto trx = storageManager->transaction();
  auto f = trx->removeFront(LogIndex{50});
  auto syncIndexBefore = storageManager->getSyncIndex();

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  auto syncIndexAfter = storageManager->getSyncIndex();
  EXPECT_EQ(syncIndexBefore, syncIndexAfter);

  EXPECT_EQ(methods.log.size(), 50);  // [50, 100)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{50});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{99});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{50}, LogIndex{100}}));
}

TEST_F(StorageManagerTest, transaction_remove_back) {
  auto trx = storageManager->transaction();
  auto f = trx->removeBack(LogIndex{50});
  auto syncIndexBefore = storageManager->getSyncIndex();

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  auto syncIndexAfter = storageManager->getSyncIndex();
  EXPECT_EQ(syncIndexBefore, syncIndexAfter);

  EXPECT_EQ(methods.log.size(), 49);  // [1, 50)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{1});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{49});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{1}, LogIndex{50}}));
}

TEST_F(StorageManagerTest, concurrent_remove_front_back) {
  auto f1 = [&] {
    auto trx = storageManager->transaction();
    return trx->removeBack(LogIndex{70});
  }();

  auto f2 = [&] {
    auto trx = storageManager->transaction();
    return trx->removeFront(LogIndex{40});
  }();

  EXPECT_FALSE(f1.isReady());
  EXPECT_FALSE(f2.isReady());
  executor->runAll();
  ASSERT_TRUE(f1.isReady());
  ASSERT_TRUE(f2.isReady());

  EXPECT_EQ(methods.log.size(), 30);  // [40, 70)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{40});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{69});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{40}, LogIndex{70}}));
}

namespace {
auto makeRange(LogTerm term, LogRange range) -> InMemoryLog {
  InMemoryLog::log_type log;
  auto transient = log.transient();
  for (auto idx : range) {
    transient.push_back(InMemoryLogEntry{
        LogEntry{term, idx, LogPayload::createFromString("")}});
  }
  return InMemoryLog(transient.persistent());
}

// we simulate a PersistedLogIterator on top of an InMemoryLog
struct InMemoryPersistedLogIterator : PersistedLogIterator {
  explicit InMemoryPersistedLogIterator(InMemoryLog log)
      : _iter(std::move(log).copyFlexVector()) {}

  auto next() -> std::optional<PersistedLogEntry> override {
    auto e = _iter.next();
    if (e) {
      return PersistedLogEntry{
          LogEntry{e->entry()},
          storage::IteratorPosition::fromLogIndex(e->entry().logIndex())};
    }
    return std::nullopt;
  }

 private:
  InMemoryLogIteratorImpl _iter;
};

}  // namespace

TEST_F(StorageManagerTest, transaction_append) {
  auto trx = storageManager->transaction();
  auto syncIndexBefore = storageManager->getSyncIndex();
  auto f =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                         {.waitForSync = true});

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  // The sync index is expected to increase by the number of entries appended.
  auto syncIndexAfter = storageManager->getSyncIndex();
  EXPECT_GT(syncIndexAfter, syncIndexBefore);
  EXPECT_EQ(syncIndexAfter, methods.log.rbegin()->first);

  EXPECT_EQ(methods.log.size(), 119);  // [1, 120)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{1});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{119});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{1}, LogIndex{120}}));
}

TEST_F(StorageManagerTest, transaction_remove_back_append) {
  {
    auto trx = storageManager->transaction();
    auto f = trx->removeBack(LogIndex{1});

    EXPECT_FALSE(f.isReady());
    executor->runOnce();
    ASSERT_TRUE(f.isReady());
  }

  auto trx = storageManager->transaction();
  auto f =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                         {.waitForSync = true});

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  EXPECT_EQ(methods.log.size(), 20);  // [100, 120)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{100});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{119});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{100}, LogIndex{120}}));
}

TEST_F(StorageManagerTest, read_meta_data) {
  auto trx = storageManager->beginMetaInfoTrx();
  EXPECT_EQ(trx->get().stateId, LogId{12});
}

TEST_F(StorageManagerTest, update_meta_data) {
  {
    auto trx = storageManager->beginMetaInfoTrx();
    auto& meta = trx->get();
    meta.snapshot.status = SnapshotStatus::kCompleted;
    storageManager->commitMetaInfoTrx(std::move(trx));
  }

  ASSERT_TRUE(methods.meta.has_value());
  EXPECT_EQ(methods.meta->snapshot.status, SnapshotStatus::kCompleted);

  {
    auto trx = storageManager->beginMetaInfoTrx();
    EXPECT_EQ(trx->get().snapshot.status, SnapshotStatus::kCompleted);
  }
}

TEST_F(StorageManagerTest, update_meta_data_abort) {
  {
    auto trx = storageManager->beginMetaInfoTrx();
    auto& meta = trx->get();
    meta.snapshot.status = SnapshotStatus::kCompleted;
    // just drop the trx
    trx.reset();
  }

  ASSERT_TRUE(methods.meta.has_value());
  EXPECT_EQ(methods.meta->snapshot.status, SnapshotStatus::kFailed);

  {
    auto trx = storageManager->beginMetaInfoTrx();
    EXPECT_EQ(trx->get().snapshot.status, SnapshotStatus::kFailed);
  }
}

struct StorageEngineMethodsMockFactory {
  auto create() -> std::unique_ptr<storage::tests::StorageEngineMethodsGMock> {
    auto ptr = std::make_unique<storage::tests::StorageEngineMethodsGMock>();
    lastPtr = ptr.get();

    EXPECT_CALL(*ptr, getIterator)
        .Times(1)
        .WillOnce([](storage::IteratorPosition pos) {
          auto log = makeRange(LogTerm{1}, {LogIndex{10}, LogIndex{100}});
          return std::make_unique<InMemoryPersistedLogIterator>(log);
        });

    EXPECT_CALL(*ptr, readMetadata).Times(1).WillOnce([]() {
      return storage::PersistedStateInfo{.stateId = LogId{1},
                                         .snapshot = {},
                                         .generation = {},
                                         .specification = {}};
    });

    return ptr;
  }
  auto operator->() noexcept -> storage::tests::StorageEngineMethodsGMock* {
    return lastPtr;
  }
  auto operator*() noexcept -> storage::tests::StorageEngineMethodsGMock& {
    return *lastPtr;
  }

 private:
  storage::tests::StorageEngineMethodsGMock* lastPtr{nullptr};
};

struct StorageManagerGMockTest : ::testing::Test {
  StorageEngineMethodsMockFactory methods;
  std::shared_ptr<test::SyncScheduler> scheduler =
      std::make_shared<test::SyncScheduler>();

  std::shared_ptr<StorageManager> storageManager =
      std::make_shared<StorageManager>(methods.create(),
                                       LoggerContext{Logger::FIXME}, scheduler);

  using StorageEngineFuture =
      futures::Promise<ResultT<storage::IStorageEngineMethods::SequenceNumber>>;
};

TEST_F(StorageManagerGMockTest, multiple_actions_with_error) {
  std::optional<StorageEngineFuture> p1;

  EXPECT_CALL(*methods, removeFront)
      .Times(1)
      .WillOnce([&p1](LogIndex stop,
                      storage::IStorageEngineMethods::WriteOptions const&) {
        return p1.emplace().getFuture();
      });

  auto trx = storageManager->transaction();
  auto f1 = trx->removeFront(LogIndex(20));

  auto trx2 = storageManager->transaction();
  auto f2 = trx2->removeBack(LogIndex{80});

  EXPECT_FALSE(f1.isReady());
  EXPECT_FALSE(f2.isReady());

  // resolve first promise with error
  p1->setValue(Result{TRI_ERROR_DEBUG});

  // first one failed with original error
  ASSERT_TRUE(f1.isReady());
  EXPECT_EQ(f1.waitAndGet().errorNumber(), TRI_ERROR_DEBUG);

  // others are aborted due to conflict
  ASSERT_TRUE(f2.isReady());
  EXPECT_EQ(f2.waitAndGet().errorNumber(),
            TRI_ERROR_REPLICATION_REPLICATED_LOG_SUBSEQUENT_FAULT);
}

TEST_F(StorageManagerGMockTest, resign_calls_barrier) {
  std::optional<StorageEngineFuture> p1;
  EXPECT_CALL(*methods, waitForCompletion).Times(1);
  std::ignore = storageManager->resign();
}

struct StorageManagerSyncIndexTest : ::testing::Test {
  StorageEngineMethodsMockFactory methods;
  std::shared_ptr<test::SyncScheduler> scheduler =
      std::make_shared<test::SyncScheduler>();

  std::shared_ptr<StorageManager> storageManager;

  using StorageEngineFuture =
      futures::Promise<ResultT<IStorageEngineMethods::SequenceNumber>>;

  auto makeStorageManager() -> std::shared_ptr<StorageManager> {
    return std::make_shared<StorageManager>(
        methods.create(), LoggerContext{Logger::FIXME}, scheduler);
  }

  void SetUp() override { storageManager = makeStorageManager(); }
};

TEST_F(StorageManagerSyncIndexTest, wait_for_sync_false_index_update) {
  IStorageEngineMethods::SequenceNumber seqNumber{1};
  auto syncIndex1 = storageManager->getSyncIndex();

  EXPECT_CALL(*methods, insert)
      .Times(2)
      .WillRepeatedly([&](std::unique_ptr<LogIterator> ptr,
                          IStorageEngineMethods::WriteOptions const& options) {
        StorageEngineFuture promise;
        promise.setValue(ResultT<decltype(seqNumber)>{seqNumber});
        return promise.getFuture();
      });

  futures::Promise<Result> lowerIndex;
  EXPECT_CALL(*methods, waitForSync(seqNumber))
      .WillOnce([&](IStorageEngineMethods::SequenceNumber number) {
        return lowerIndex.getFuture();
      });

  /*
   * appendEntries works by calling scheduleOperation.
   * This returns a Future<Result> that is supposed to be resolved once
   * methods->insert is resolved. However, after the insert is resolved, a new
   * lambda is scheduled, eventually resolving the appendEntries future at a
   * later time.
   */

  auto trx1 = storageManager->transaction();
  auto f1 =
      trx1->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                          {.waitForSync = false});

  // Resolve the appendEntries future, but not the waitForSync future
  EXPECT_TRUE(f1.isReady());
  ++seqNumber;
  auto syncIndex2 = storageManager->getSyncIndex();
  EXPECT_EQ(syncIndex2, syncIndex1);

  // A new appendEntries, this time resolving the waitForSync future
  futures::Promise<Result> higherIndex;
  EXPECT_CALL(*methods, waitForSync(seqNumber))
      .WillOnce([&](IStorageEngineMethods::SequenceNumber number) {
        return higherIndex.getFuture();
      });
  auto trx2 = storageManager->transaction();
  auto f2 =
      trx2->appendEntries(makeRange(LogTerm{1}, {LogIndex{120}, LogIndex{140}}),
                          {.waitForSync = false});
  EXPECT_TRUE(f2.isReady());
  higherIndex.setValue(Result{});
  auto syncIndex3 = storageManager->getSyncIndex();
  EXPECT_GT(syncIndex3, syncIndex1);

  // Since this is a lower index, it should not have any effect on the syncIndex
  lowerIndex.setValue(Result{});
  auto syncIndex4 = storageManager->getSyncIndex();
  EXPECT_EQ(syncIndex4, syncIndex3);
}

TEST_F(StorageManagerSyncIndexTest, wait_for_sync_false_update_fails) {
  IStorageEngineMethods::SequenceNumber seqNumber{1};
  auto syncIndex1 = storageManager->getSyncIndex();

  EXPECT_CALL(*methods, insert)
      .WillOnce([&](std::unique_ptr<LogIterator> ptr,
                    IStorageEngineMethods::WriteOptions const& options) {
        StorageEngineFuture promise;
        promise.setValue(ResultT<decltype(seqNumber)>{seqNumber});
        return promise.getFuture();
      });

  EXPECT_CALL(*methods, waitForSync(seqNumber))
      .WillOnce([&](IStorageEngineMethods::SequenceNumber number) {
        futures::Promise<Result> promise;
        promise.setValue(Result{TRI_ERROR_WAS_ERLAUBE});
        return promise.getFuture();
      });

  auto trx = storageManager->transaction();
  std::ignore =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                         {.waitForSync = false});
  auto syncIndex2 = storageManager->getSyncIndex();
  EXPECT_EQ(syncIndex2, syncIndex1);
}

TEST_F(StorageManagerSyncIndexTest, manager_unavailable_during_update) {
  IStorageEngineMethods::SequenceNumber seqNumber{1};

  EXPECT_CALL(*methods, insert)
      .WillOnce([&](std::unique_ptr<LogIterator> ptr,
                    IStorageEngineMethods::WriteOptions const& options) {
        StorageEngineFuture promise;
        promise.setValue(ResultT<decltype(seqNumber)>{seqNumber});
        return promise.getFuture();
      });

  futures::Promise<Result> wfsPromise;
  EXPECT_CALL(*methods, waitForSync(seqNumber))
      .WillOnce([&](IStorageEngineMethods::SequenceNumber number) {
        return wfsPromise.getFuture();
      });

  auto trx = storageManager->transaction();
  std::ignore =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                         {.waitForSync = false});
  // In case the manager is unavailable, we should not panic.
  storageManager.reset();
  wfsPromise.setValue(Result{});
}

TEST_F(StorageManagerSyncIndexTest, methods_insertion_fails) {
  EXPECT_CALL(*methods, insert)
      .WillOnce([](std::unique_ptr<LogIterator> ptr,
                   IStorageEngineMethods::WriteOptions const& options) {
        StorageEngineFuture promise;
        promise.setValue(Result{TRI_ERROR_WAS_ERLAUBE});
        return promise.getFuture();
      });
  EXPECT_CALL(*methods, waitForSync(testing::_)).Times(0);
  auto trx = storageManager->transaction();
  std::ignore =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}),
                         {.waitForSync = false});
}
