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
#include <immer/flex_vector_transient.hpp>

#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

struct StorageManagerTest : ::testing::Test {
  std::uint64_t const objectId = 1;
  LogId const logId = LogId{12};
  std::shared_ptr<test::DelayedExecutor> executor =
      std::make_shared<test::DelayedExecutor>();
  test::FakeStorageEngineMethodsContext methods{
      objectId, logId, executor, LogRange{LogIndex{1}, LogIndex{100}}};
  std::shared_ptr<StorageManager> storageManager =
      std::make_shared<StorageManager>(methods.getMethods());
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

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

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

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  EXPECT_EQ(methods.log.size(), 49);  // [1, 50)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{1});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{49});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{1}, LogIndex{49}}));
}

namespace {
auto makeRange(LogTerm term, LogRange range) -> InMemoryLog {
  InMemoryLog::log_type log;
  auto transient = log.transient();
  for (auto idx : range) {
    transient.push_back(InMemoryLogEntry{
        PersistingLogEntry{term, idx, LogPayload::createFromString("")}});
  }
  return InMemoryLog(transient.persistent());
}
}  // namespace

TEST_F(StorageManagerTest, transaction_append) {
  auto trx = storageManager->transaction();
  auto f =
      trx->appendEntries(makeRange(LogTerm{1}, {LogIndex{100}, LogIndex{120}}));

  EXPECT_FALSE(f.isReady());
  executor->runOnce();
  ASSERT_TRUE(f.isReady());

  EXPECT_EQ(methods.log.size(), 119);  // [1, 120)
  EXPECT_EQ(methods.log.begin()->first, LogIndex{1});
  EXPECT_EQ(methods.log.rbegin()->first, LogIndex{119});

  auto trx2 = storageManager->transaction();
  auto logBounds = trx2->getLogBounds();
  EXPECT_EQ(logBounds, (LogRange{LogIndex{1}, LogIndex{120}}));
}
