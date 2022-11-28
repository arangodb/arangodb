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

#include "Replication2/ReplicatedLog/Components/LogFollower2.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/ReplicatedState/StateStatus.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

namespace {
struct ReplicatedStateHandleMock : IReplicatedStateHandle {
  MOCK_METHOD(std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>,
              resignCurrentState, (), (noexcept, override));
  MOCK_METHOD(void, leadershipEstablished,
              (std::unique_ptr<IReplicatedLogLeaderMethods>), (override));
  MOCK_METHOD(void, becomeFollower,
              (std::unique_ptr<IReplicatedLogFollowerMethods>), (override));
  MOCK_METHOD(void, acquireSnapshot, (ServerID leader, LogIndex),
              (noexcept, override));
  MOCK_METHOD(void, updateCommitIndex, (LogIndex), (noexcept, override));
  MOCK_METHOD(std::optional<replicated_state::StateStatus>, getStatus, (),
              (const, override));
  MOCK_METHOD(std::shared_ptr<replicated_state::IReplicatedFollowerStateBase>,
              getFollower, (), (const, override));
  MOCK_METHOD(std::shared_ptr<replicated_state::IReplicatedLeaderStateBase>,
              getLeader, (), (const, override));
  MOCK_METHOD(void, dropEntries, (), (override));
};
}  // namespace

struct AppendEntriesFollowerTest : ::testing::Test {
  std::uint64_t const objectId = 1;
  LogId const logId = LogId{12};
  std::shared_ptr<test::SyncExecutor> executor =
      std::make_shared<test::SyncExecutor>();

  test::FakeStorageEngineMethodsContext methods{
      objectId,
      logId,
      executor,
      {LogIndex{1}, LogIndex{100}},
      replicated_state::PersistedStateInfo{
          .stateId = logId, .snapshot = {.status = SnapshotStatus::kFailed}}};

  std::shared_ptr<ReplicatedLogGlobalSettings> options =
      std::make_shared<ReplicatedLogGlobalSettings>();
  std::shared_ptr<FollowerTermInformation> termInfo =
      std::make_shared<FollowerTermInformation>();

  testing::StrictMock<ReplicatedStateHandleMock> stateHandle;

  std::shared_ptr<refactor::FollowerManager> follower =
      std::make_shared<refactor::FollowerManager>(methods.getMethods(), nullptr,
                                                  termInfo, options);
};

TEST_F(AppendEntriesFollowerTest, no_test) {}
