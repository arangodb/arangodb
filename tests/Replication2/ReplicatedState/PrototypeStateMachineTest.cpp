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

#include <array>
#include <utility>

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Replication2/StateMachines/Prototype/PrototypeStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

struct MockPrototypeLeaderInterface : public IPrototypeLeaderInterface {
  explicit MockPrototypeLeaderInterface(
      std::shared_ptr<PrototypeLeaderState> leaderState)
      : leaderState(std::move(leaderState)) {}

  auto getSnapshot(GlobalLogIdentifier const&, LogIndex waitForIndex)
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    return leaderState->getSnapshot(waitForIndex);
  }

  std::shared_ptr<PrototypeLeaderState> leaderState;
};

struct MockPrototypeNetworkInterface : public IPrototypeNetworkInterface {
  auto getLeaderInterface(ParticipantId id)
      -> ResultT<std::shared_ptr<IPrototypeLeaderInterface>> override {
    if (auto leaderState = leaderStates.find(id);
        leaderState != leaderStates.end()) {
      return ResultT<std::shared_ptr<IPrototypeLeaderInterface>>::success(
          std::make_shared<MockPrototypeLeaderInterface>(leaderState->second));
    }
    return {TRI_ERROR_CLUSTER_NOT_LEADER};
  }

  void addLeaderState(ParticipantId id,
                      std::shared_ptr<PrototypeLeaderState> leaderState) {
    leaderStates[std::move(id)] = std::move(leaderState);
  }

  std::unordered_map<ParticipantId, std::shared_ptr<PrototypeLeaderState>>
      leaderStates;
};

struct MockPrototypeStorageInterface : public IPrototypeStorageInterface {
  auto put(const GlobalLogIdentifier& logId, PrototypeCoreDump dump)
      -> Result override {
    map[logId.id] = std::move(dump);
    return TRI_ERROR_NO_ERROR;
  }
  auto get(const GlobalLogIdentifier& logId)
      -> ResultT<PrototypeCoreDump> override {
    if (!map.contains(logId.id)) {
      return ResultT<PrototypeCoreDump>::success(map[logId.id] =
                                                     PrototypeCoreDump{});
    }
    return ResultT<PrototypeCoreDump>::success(map[logId.id]);
  }

  std::unordered_map<LogId, PrototypeCoreDump> map;
};

struct PrototypeStateMachineTest : test::ReplicatedLogTest {
  PrototypeStateMachineTest() {
    feature->registerStateType<prototype::PrototypeState>(
        "prototype-state", networkMock, storageMock);
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
  std::shared_ptr<MockPrototypeNetworkInterface> networkMock =
      std::make_shared<MockPrototypeNetworkInterface>();
  std::shared_ptr<MockPrototypeStorageInterface> storageMock =
      std::make_shared<MockPrototypeStorageInterface>();
};

TEST_F(PrototypeStateMachineTest, prorotype_core_wait_for) {
  auto core = PrototypeCore(GlobalLogIdentifier{"", LogId{1}}, storageMock);
  core.store = core.store.set("a", "b");
  core.lastAppliedIndex = LogIndex{1};
  auto f = core.waitForApplied(LogIndex{1});
  ASSERT_TRUE(f.isReady());
  f = core.waitForApplied(LogIndex{3});
  ASSERT_FALSE(f.isReady());
  core.lastAppliedIndex = LogIndex{3};
  core.resolvePromises(LogIndex{3});
  ASSERT_TRUE(f.isReady());
}

TEST_F(PrototypeStateMachineTest, prorotype_core_flush) {
  auto core = PrototypeCore(GlobalLogIdentifier{"", LogId{1}}, storageMock);
  core.store = core.store.set("x", "y");
  core.store = core.store.set("a", "b");
  core.lastAppliedIndex = LogIndex{2};
  core.flush();
  auto result = storageMock->get(GlobalLogIdentifier{"", LogId{1}});
  ASSERT_TRUE(result.ok());
  auto dump = result.get();
  auto expected =
      std::unordered_map<std::string, std::string>{{"a", "b"}, {"x", "y"}};
  ASSERT_EQ(dump.map, expected);
  ASSERT_EQ(dump.lastPersistedIndex, core.lastPersistedIndex);
}

TEST_F(PrototypeStateMachineTest, simple_operations) {
  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));
  follower->runAllAsyncAppendEntries();

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  networkMock->addLeaderState("leader", leaderState);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  {
    auto entries = std::unordered_map<std::string, std::string>{{"foo", "bar"}};
    auto result = leaderState->set(entries);
    follower->runAllAsyncAppendEntries();
    auto index = result.get()->value;
    ASSERT_EQ(index, 2);
  }

  {
    auto result = leaderState->get("foo");
    ASSERT_EQ(result, "bar");
    result = leaderState->get("baz");
    ASSERT_EQ(result, std::nullopt);

    result = followerState->get("foo");
    ASSERT_EQ(result, "bar");
    result = followerState->get("baz");
    ASSERT_EQ(result, std::nullopt);
  }

  {
    std::initializer_list<std::pair<std::string, std::string>> values1{
        {"foo1", "bar1"}, {"foo2", "bar2"}, {"foo3", "bar3"}};
    auto result = leaderState->set(values1.begin(), values1.end());
    follower->runAllAsyncAppendEntries();
    auto index = result.get()->value;
    ASSERT_EQ(index, 3);
  }

  {
    std::array<std::string, 4> entries = {"foo1", "foo2", "foo3", "nofoo"};
    std::unordered_map<std::string, std::string> result =
        leaderState->get(entries.begin(), entries.end());
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result["foo1"], "bar1");
    ASSERT_EQ(followerState->get("foo1"), "bar1");
  }

  {
    auto result = leaderState->remove("foo1");
    follower->runAllAsyncAppendEntries();
    auto index = result.get()->value;
    ASSERT_EQ(index, 4);
    ASSERT_EQ(leaderState->get("foo1"), std::nullopt);
  }

  {
    std::vector<std::string> entries = {"nofoo", "foo2"};
    auto result = leaderState->remove(entries);
    follower->runAllAsyncAppendEntries();
    auto index = result.get()->value;
    ASSERT_EQ(index, 5);
    ASSERT_EQ(leaderState->get("foo2"), std::nullopt);
    ASSERT_EQ(leaderState->get("foo3"), "bar3");
    ASSERT_EQ(followerState->get("foo2"), std::nullopt);
    ASSERT_EQ(followerState->get("foo3"), "bar3");
  }

  {
    auto result = leaderState->getSnapshot(LogIndex{3});
    ASSERT_TRUE(result.isReady());
    auto map = result.get();
    auto expected = std::unordered_map<std::string, std::string>{
        {"foo", "bar"}, {"foo3", "bar3"}};
    ASSERT_EQ(map, expected);
    ASSERT_EQ(followerState->get("foo"), "bar");
    ASSERT_EQ(followerState->get("foo3"), "bar3");
  }
}

TEST_F(PrototypeStateMachineTest, snapshot_transfer) {
  auto follower1Log = makeReplicatedLog(LogId{1});
  auto follower1 =
      follower1Log->becomeFollower("follower1", LogTerm{1}, "leader");

  auto follower2Log = makeReplicatedLog(LogId{1});
  auto follower2 =
      follower2Log->becomeFollower("follower2", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader =
      leaderLog->becomeLeader("leader", LogTerm{1}, {follower1, follower2}, 2);

  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));
  follower1->runAllAsyncAppendEntries();
  follower2->runAllAsyncAppendEntries();

  auto leaderState = leaderReplicatedState->getLeader();
  networkMock->addLeaderState("leader", leaderState);
  ASSERT_NE(leaderState, nullptr);

  auto followerReplicatedState1 =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", follower1Log));
  ASSERT_NE(followerReplicatedState1, nullptr);
  followerReplicatedState1->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  auto followerState1 = followerReplicatedState1->getFollower();
  ASSERT_NE(followerState1, nullptr);

  {
    auto result = leaderState->getSnapshot(LogIndex{2});
    ASSERT_FALSE(result.isReady());

    auto entries = std::unordered_map<std::string, std::string>{
        {"foo1", "bar1"}, {"foo2", "bar2"}, {"foo3", "bar3"}};
    leaderState->set(entries);
    follower1->runAllAsyncAppendEntries();

    ASSERT_TRUE(result.isReady());
    auto map = result.get().get();
    ASSERT_EQ(map, entries);
  }

  {
    auto result = leaderState->getSnapshot(LogIndex{4});
    ASSERT_FALSE(result.isReady());

    auto fut1 = leaderState->set({{"foo4", "bar4"}});
    auto fut2 = leaderState->remove("foo4");
    follower1->runAllAsyncAppendEntries();
    fut1.wait();
    fut2.wait();

    ASSERT_TRUE(result.isReady());
    auto map = result.get().get();

    auto expected = std::unordered_map<std::string, std::string>{
        {"foo1", "bar1"}, {"foo2", "bar2"}, {"foo3", "bar3"}};
    ASSERT_EQ(map, expected);
  }

  auto followerReplicatedState2 =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", follower2Log));
  ASSERT_NE(followerReplicatedState2, nullptr);
  followerReplicatedState2->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  follower2->runAllAsyncAppendEntries();
  auto followerState2 = followerReplicatedState2->getFollower();
  ASSERT_NE(followerState2, nullptr);
  {
    auto map = followerState2->dumpContent();
    auto expected = std::unordered_map<std::string, std::string>{
        {"foo1", "bar1"}, {"foo2", "bar2"}, {"foo3", "bar3"}};
    ASSERT_EQ(map, expected);
  }
}
