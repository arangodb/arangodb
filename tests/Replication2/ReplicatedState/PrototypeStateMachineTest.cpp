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

#include "Replication2/StateMachines/Prototype/PrototypeCore.h"
#include "Replication2/StateMachines/Prototype/PrototypeFollowerState.h"
#include "Replication2/StateMachines/Prototype/PrototypeLeaderState.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMachine.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMethods.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

namespace {
struct MockPrototypeLeaderInterface : public IPrototypeLeaderInterface {
  explicit MockPrototypeLeaderInterface(
      std::shared_ptr<PrototypeLeaderState> leaderState,
      bool useDefaultSnapshot)
      : leaderState(std::move(leaderState)),
        useDefaultSnapshot(useDefaultSnapshot),
        defaultSnapshot{{"a", "b"}, {"c", "d"}} {}

  auto getSnapshot(GlobalLogIdentifier const&, LogIndex waitForIndex)
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> override {
    if (useDefaultSnapshot) {
      return ResultT<std::unordered_map<std::string, std::string>>::success(
          defaultSnapshot);
    }
    return leaderState->getSnapshot(waitForIndex);
  }

  std::shared_ptr<PrototypeLeaderState> leaderState;
  bool useDefaultSnapshot;
  std::unordered_map<std::string, std::string> defaultSnapshot;
};

struct MockPrototypeNetworkInterface : public IPrototypeNetworkInterface {
  auto getLeaderInterface(ParticipantId id)
      -> ResultT<std::shared_ptr<IPrototypeLeaderInterface>> override {
    if (auto leaderState = leaderStates.find(id);
        leaderState != leaderStates.end()) {
      return ResultT<std::shared_ptr<IPrototypeLeaderInterface>>::success(
          std::make_shared<MockPrototypeLeaderInterface>(leaderState->second,
                                                         useDefaultSnapshot));
    }
    return {TRI_ERROR_CLUSTER_NOT_LEADER};
  }

  void addLeaderState(ParticipantId id,
                      std::shared_ptr<PrototypeLeaderState> leaderState) {
    leaderStates.emplace(std::move(id), std::move(leaderState));
  }

  bool useDefaultSnapshot = false;
  std::unordered_map<ParticipantId, std::shared_ptr<PrototypeLeaderState>>
      leaderStates;
};

struct MockPrototypeStorageInterface : public IPrototypeStorageInterface {
  auto put(const GlobalLogIdentifier& logId, PrototypeDump dump)
      -> Result override {
    map[logId.id] = std::move(dump);
    ++putCalled;
    return TRI_ERROR_NO_ERROR;
  }

  auto get(const GlobalLogIdentifier& logId)
      -> ResultT<PrototypeDump> override {
    if (!map.contains(logId.id)) {
      return ResultT<PrototypeDump>::success(map[logId.id] = PrototypeDump{});
    }
    return ResultT<PrototypeDump>::success(map[logId.id]);
  }

  std::unordered_map<LogId, PrototypeDump> map;
  int putCalled{0};
};
}  // namespace

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

TEST_F(PrototypeStateMachineTest, prototype_core_flush) {
  auto logId = LogId{1};
  auto followerLog = makeReplicatedLog(logId);
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(logId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);
  follower->runAllAsyncAppendEntries();

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  networkMock->addLeaderState("leader", leaderState);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  std::unordered_map<std::string, std::string> expected;
  for (std::size_t cnt{0}; cnt < PrototypeCore::kFlushBatchSize; ++cnt) {
    auto key = "foo" + std::to_string(cnt);
    auto value = "bar" + std::to_string(cnt);
    auto entries = std::unordered_map<std::string, std::string>{{key, value}};
    expected.emplace(key, value);
    auto result = leaderState->set(
        entries,
        PrototypeStateMethods::PrototypeWriteOptions{.waitForApplied = false});
    ASSERT_TRUE(result.isReady());
    auto index = result.get().value;
    ASSERT_EQ(index, cnt + 2);
  }
  follower->runAllAsyncAppendEntries();

  // put is called twice, once from the leader and once from the follower
  ASSERT_EQ(storageMock->putCalled, 2);

  auto snapshot = leaderState->getSnapshot(LogIndex{1});
  ASSERT_TRUE(snapshot.isReady());
  auto leaderMap = snapshot.get().get();
  ASSERT_EQ(expected, leaderMap);

  auto prototypeDump = storageMock->get(GlobalLogIdentifier{"database", logId});
  ASSERT_EQ(prototypeDump.get().map, leaderMap);
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
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);
  follower->runAllAsyncAppendEntries();

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  networkMock->addLeaderState("leader", leaderState);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  decltype(LogIndex::value) index{0};
  PrototypeStateMethods::PrototypeWriteOptions options{};

  // Compare-exchange before insert
  {
    auto result =
        leaderState->compareExchange("cmp", "cmp1", "cmp2", options).get();
    ASSERT_EQ(result.errorNumber(), TRI_ERROR_ARANGO_CONFLICT);
  }

  // Get before insert
  {
    auto result = leaderState->get("baz", LogIndex{index}).get();
    ASSERT_EQ(result.get(), std::nullopt);
    result = followerState->get("baz", LogIndex{index}).get();
    ASSERT_EQ(result.get(), std::nullopt);
  }

  // Inserting one entry
  {
    auto entries = std::unordered_map<std::string, std::string>{{"foo", "bar"}};
    auto result = leaderState->set(std::move(entries), options);
    follower->runAllAsyncAppendEntries();
    index = result.get().value;
    ASSERT_EQ(index, 2);
  }

  // Single get
  {
    auto result = leaderState->get("foo", LogIndex{index}).get();
    ASSERT_EQ(result.get(), "bar");
    result = leaderState->get("baz", LogIndex{index}).get();
    ASSERT_EQ(result.get(), std::nullopt);

    result = followerState->get("foo", LogIndex{index}).get();
    ASSERT_EQ(result.get(), "bar");
    result = followerState->get("baz", LogIndex{index}).get();
    ASSERT_EQ(result.get(), std::nullopt);
  }

  // Inserting multiple entries
  {
    std::unordered_map<std::string, std::string> entries{
        {"foo1", "bar1"}, {"foo2", "bar2"}, {"foo3", "bar3"}};
    auto result = leaderState->set(std::move(entries), options);
    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(result.isReady());
    index = result.get().value;
    ASSERT_EQ(index, 3);
  }

  // Getting multiple entries
  {
    std::vector<std::string> entries = {"foo1", "foo2", "foo3", "nofoo"};
    std::unordered_map<std::string, std::string> result =
        leaderState->get(entries, LogIndex{index}).get().get();
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result["foo1"], "bar1");
    ASSERT_EQ(result["foo2"], "bar2");
    ASSERT_EQ(result["foo3"], "bar3");

    result = followerState->get(entries, LogIndex{index}).get().get();
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result["foo1"], "bar1");
    ASSERT_EQ(result["foo2"], "bar2");
    ASSERT_EQ(result["foo3"], "bar3");
  }

  // Removing single entry
  {
    auto result = leaderState->remove("foo1", options);
    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(result.isReady());
    index = result.get().value;
    ASSERT_EQ(index, 4);
    ASSERT_EQ(leaderState->get("foo1", LogIndex{index}).get().get(),
              std::nullopt);
  }

  // Removing multiple entries
  {
    std::vector<std::string> entries = {"nofoo", "foo2"};
    auto result = leaderState->remove(std::move(entries), options);
    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(result.isReady());
    index = result.get().value;
    ASSERT_EQ(index, 5);
    ASSERT_EQ(leaderState->get("foo2", LogIndex{index}).get().get(),
              std::nullopt);
    ASSERT_EQ(leaderState->get("foo3", LogIndex{index}).get().get(), "bar3");
    ASSERT_EQ(followerState->get("foo2", LogIndex{index}).get().get(),
              std::nullopt);
    ASSERT_EQ(followerState->get("foo3", LogIndex{index}).get().get(), "bar3");
  }

  // Compare Exchange
  {
    auto wrongValue =
        leaderState->compareExchange("foo3", "foobar", "foobar", options).get();
    ASSERT_EQ(wrongValue.errorNumber(), TRI_ERROR_ARANGO_CONFLICT);
    auto result =
        leaderState->compareExchange("foo3", "bar3", "foobar", options);
    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(result.isReady());
    index = result.get().get().value;
    ASSERT_EQ(index, 6);
  }

  // Check final state
  {
    auto result = leaderState->getSnapshot(LogIndex{3});
    ASSERT_TRUE(result.isReady());
    auto map = result.get();
    auto expected = std::unordered_map<std::string, std::string>{
        {"foo", "bar"}, {"foo3", "foobar"}};
    ASSERT_EQ(map, expected);
    ASSERT_EQ(followerState->get("foo", LogIndex{index}).get().get(), "bar");
    ASSERT_EQ(followerState->get("foo3", LogIndex{index}).get().get(),
              "foobar");
  }
}

TEST_F(PrototypeStateMachineTest, snapshot_transfer) {
  networkMock->useDefaultSnapshot = true;
  auto logId = LogId{1};
  auto followerLog = makeReplicatedLog(logId);
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(logId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);
  follower->runAllAsyncAppendEntries();

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  networkMock->addLeaderState("leader", leaderState);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), std::nullopt);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);
  ASSERT_EQ(followerState->get("a", LogIndex{0}).get().get(), "b");
  ASSERT_EQ(followerState->get("c", LogIndex{0}).get().get(), "d");
}
