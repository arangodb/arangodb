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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateStrategy.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

struct MockDocumentStateAgencyHandler : public IDocumentStateAgencyHandler {
  auto getCollectionPlan(std::string const& database,
                         std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override {
    return std::make_shared<VPackBuilder>();
  }

  auto reportShardInCurrent(
      std::string const& database, std::string const& collectionId,
      std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override {
    shards.emplace_back(shardId, collectionId);
    return {};
  }

  std::vector<std::pair<std::string, std::string>> shards;
};

struct MockDocumentStateShardHandler : public IDocumentStateShardHandler {
  MockDocumentStateShardHandler() : shardId{0} {};

  auto createLocalShard(GlobalLogIdentifier const& gid,
                        std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override {
    ++shardId;
    return ResultT<std::string>::success(std::to_string(shardId));
  }

  int shardId;
};

struct DocumentStateMachineTest : test::ReplicatedLogTest {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              agencyHandler, shardHandler);
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
  std::shared_ptr<MockDocumentStateAgencyHandler> agencyHandler =
      std::make_shared<MockDocumentStateAgencyHandler>();
  std::shared_ptr<MockDocumentStateShardHandler> shardHandler =
      std::make_shared<MockDocumentStateShardHandler>();
};

TEST_F(DocumentStateMachineTest, simple_operations) {
  const std::string collectionId = "testCollectionID";

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();

  auto parameters =
      document::DocumentCoreParameters{collectionId}.toSharedSlice();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), parameters);
  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(shardHandler->shardId, 1);
  ASSERT_EQ(agencyHandler->shards.size(), 1);
  ASSERT_EQ(agencyHandler->shards[0].first, "1");
  ASSERT_EQ(agencyHandler->shards[0].second, collectionId);

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), parameters);
  ASSERT_EQ(shardHandler->shardId, 2);
  ASSERT_EQ(agencyHandler->shards.size(), 2);
  ASSERT_EQ(agencyHandler->shards[1].first, "2");
  ASSERT_EQ(agencyHandler->shards[1].second, collectionId);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);
}