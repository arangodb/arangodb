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

#include "Replication2/ReplicatedLog/Components/FollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Basics/Result.h"
#include "Replication2/Mocks/SchedulerMocks.h"
#include "Replication2/Mocks/StateHandleManagerMock.h"
#include "Replication2/Mocks/StorageManagerMock.h"
#include "Replication2/Storage/IteratorPosition.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::comp;

namespace {

auto makeRange(LogRange range) -> TermIndexMapping {
  TermIndexMapping mapping;
  mapping.insert(range, storage::IteratorPosition::fromLogIndex(range.from),
                 LogTerm{1});
  return mapping;
}

auto makeRangeIter(LogRange range) -> std::unique_ptr<LogViewRangeIterator> {
  InMemoryLog::log_type log;
  auto transient = log.transient();
  for (auto idx : range) {
    transient.push_back(InMemoryLogEntry{
        LogEntry{LogTerm{1}, idx, LogPayload::createFromString("")}});
  }
  return InMemoryLog(transient.persistent()).getIteratorRange(range);
}

}  // namespace

struct FollowerCommitManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storage;
  testing::StrictMock<StateHandleManagerMock> stateHandle;

  std::shared_ptr<test::SyncScheduler> scheduler =
      std::make_shared<test::SyncScheduler>();

  std::shared_ptr<FollowerCommitManager> commit =
      std::make_shared<FollowerCommitManager>(
          storage, LoggerContext{Logger::REPLICATION2}, scheduler);
};

TEST_F(FollowerCommitManagerTest, wait_for_update_commit_index) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitFor(LogIndex{12});
  EXPECT_FALSE(f.isReady());
  auto&& [resolveIndex, action] = commit->updateCommitIndex(LogIndex{12}, true);
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{12});

  ASSERT_TRUE(f.isReady());
  auto index = f.waitAndGet().currentCommitIndex;
  EXPECT_EQ(index, LogIndex{12});
}

TEST_F(FollowerCommitManagerTest, wait_for_iterator_update_commit_index) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitForIterator(LogIndex{12});
  EXPECT_FALSE(f.isReady());

  EXPECT_CALL(storage, getCommittedLogIterator)
      .Times(1)
      .WillOnce([](std::optional<LogRange> bounds) {
        EXPECT_EQ(bounds, (LogRange{LogIndex{12}, LogIndex{26}}));
        return makeRangeIter(bounds.value());
      });

  auto&& [resolveIndex, action] = commit->updateCommitIndex(LogIndex{25}, true);
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{25});

  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).waitAndGet();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{26}}));  // contains the interval [12, 25]
}

TEST_F(FollowerCommitManagerTest, wait_for_update_commit_index_missing_log) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitFor(LogIndex{50});
  EXPECT_FALSE(f.isReady());
  auto&& [resolveIndex, action] = commit->updateCommitIndex(LogIndex{60}, true);
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{44});
  // although commit index is 60, we have log upto 45
  // so waiting for 50 should not be resolved

  EXPECT_FALSE(f.isReady());
}

TEST_F(FollowerCommitManagerTest,
       wait_for_iterator_update_commit_index_missing_log) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitForIterator(LogIndex{12});
  EXPECT_FALSE(f.isReady());

  EXPECT_CALL(storage, getCommittedLogIterator)
      .Times(1)
      .WillOnce([](std::optional<LogRange> bounds) {
        EXPECT_EQ(bounds, (LogRange{LogIndex{12}, LogIndex{45}}));
        return makeRangeIter(bounds.value());
      });

  auto&& [resolveIndex, action] = commit->updateCommitIndex(
      LogIndex{60}, true);  // return only upto 45, although 60
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{44});
  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).waitAndGet();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{45}}));  // contains the interval [12, 45]
}

TEST_F(FollowerCommitManagerTest, wait_for_already_resolved) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillRepeatedly([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto&& [resolveIndex, action] = commit->updateCommitIndex(LogIndex{30}, true);
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{30});
  auto f = commit->waitFor(LogIndex{12});

  ASSERT_TRUE(f.isReady());
  EXPECT_EQ(f.waitAndGet().currentCommitIndex,
            LogIndex{30});  // contains the interval [12, 45]
}

TEST_F(FollowerCommitManagerTest, wait_for_iterator_already_resolved) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillRepeatedly([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  EXPECT_CALL(storage, getCommittedLogIterator)
      .Times(1)
      .WillOnce([](std::optional<LogRange> bounds) {
        EXPECT_EQ(bounds, (LogRange{LogIndex{12}, LogIndex{31}}));
        return makeRangeIter(bounds.value());
      });

  auto&& [resolveIndex, action] = commit->updateCommitIndex(LogIndex{30}, true);
  action.fire();
  EXPECT_EQ(resolveIndex, LogIndex{30});
  auto f = commit->waitForIterator(LogIndex{12});

  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).waitAndGet();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{31}}));  // contains the interval [12, 30]
}

TEST_F(FollowerCommitManagerTest, wait_for_snapshot) {
  EXPECT_CALL(storage, getTermIndexMapping).Times(1).WillRepeatedly([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitFor(LogIndex{12});

  auto&& [resolveIndex, action] =
      commit->updateCommitIndex(LogIndex{30}, false);
  action.fire();
  EXPECT_EQ(resolveIndex, std::nullopt);

  ASSERT_FALSE(f.isReady());
}
