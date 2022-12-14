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

#include "Replication2/ReplicatedLog/Components/FollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Basics/Result.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::comp;

namespace {
struct StorageManagerMock : IStorageManager {
  MOCK_METHOD(std::unique_ptr<IStorageTransaction>, transaction, (),
              (override));
  MOCK_METHOD(InMemoryLog, getCommittedLog, (), (const, override));
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
  MOCK_METHOD(void, acquireSnapshot, (ParticipantId const&),
              (noexcept, override));
};

auto makeRange(LogRange range) -> InMemoryLog {
  InMemoryLog::log_type log;
  auto transient = log.transient();
  for (auto idx : range) {
    transient.push_back(InMemoryLogEntry{
        PersistingLogEntry{LogTerm{1}, idx, LogPayload::createFromString("")}});
  }
  return InMemoryLog(transient.persistent());
}

}  // namespace

struct FollowerCommitManagerTest : ::testing::Test {
  testing::StrictMock<StorageManagerMock> storage;
  testing::StrictMock<StateHandleManagerMock> stateHandle;

  std::shared_ptr<FollowerCommitManager> commit =
      std::make_shared<FollowerCommitManager>(
          storage, stateHandle, LoggerContext{Logger::REPLICATION2});
};

TEST_F(FollowerCommitManagerTest, wait_for_update_commit_index) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{12})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitFor(LogIndex{12});
  EXPECT_FALSE(f.isReady());
  commit->updateCommitIndex(LogIndex{12});

  ASSERT_TRUE(f.isReady());
  auto index = f.get().currentCommitIndex;
  EXPECT_EQ(index, LogIndex{12});
}

TEST_F(FollowerCommitManagerTest, wait_for_iterator_update_commit_index) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{25})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitForIterator(LogIndex{12});
  EXPECT_FALSE(f.isReady());
  commit->updateCommitIndex(LogIndex{25});

  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).get();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{26}}));  // contains the interval [12, 25]
}

TEST_F(FollowerCommitManagerTest, wait_for_update_commit_index_missing_log) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{44})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitFor(LogIndex{50});
  EXPECT_FALSE(f.isReady());
  commit->updateCommitIndex(LogIndex{60});
  // although commit index is 60, we have log upto 45
  // so waiting for 50 should not be resolved

  EXPECT_FALSE(f.isReady());
}

TEST_F(FollowerCommitManagerTest,
       wait_for_iterator_update_commit_index_missing_log) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{44})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(1).WillOnce([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  auto f = commit->waitForIterator(LogIndex{12});
  EXPECT_FALSE(f.isReady());
  commit->updateCommitIndex(LogIndex{60});  // return only upto 45, although 60

  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).get();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{45}}));  // contains the interval [12, 45]
}

TEST_F(FollowerCommitManagerTest, wait_for_already_resolved) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{30})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(2).WillRepeatedly([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  commit->updateCommitIndex(LogIndex{30});
  auto f = commit->waitFor(LogIndex{12});

  ASSERT_TRUE(f.isReady());
  EXPECT_EQ(f.get().currentCommitIndex,
            LogIndex{30});  // contains the interval [12, 45]
}

TEST_F(FollowerCommitManagerTest, wait_for_iterator_already_resolved) {
  EXPECT_CALL(stateHandle, updateCommitIndex(LogIndex{30})).Times(1);
  EXPECT_CALL(storage, getCommittedLog).Times(2).WillRepeatedly([] {
    return makeRange({LogIndex{10}, LogIndex{45}});
  });

  commit->updateCommitIndex(LogIndex{30});
  auto f = commit->waitForIterator(LogIndex{12});

  ASSERT_TRUE(f.isReady());
  auto iter = std::move(f).get();
  EXPECT_EQ(iter->range(),
            (LogRange{LogIndex{12},
                      LogIndex{31}}));  // contains the interval [12, 30]
}
