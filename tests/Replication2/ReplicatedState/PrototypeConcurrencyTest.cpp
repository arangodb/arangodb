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

#include <array>

#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMachine.h"
#include "Replication2/StateMachines/Prototype/PrototypeLeaderState.h"
#include "Replication2/Mocks/AsyncFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

namespace {
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
  auto put(const GlobalLogIdentifier& logId, PrototypeDump dump)
      -> Result override {
    map[logId.id] = std::move(dump);
    return TRI_ERROR_NO_ERROR;
  }

  auto get(const GlobalLogIdentifier& logId)
      -> ResultT<PrototypeDump> override {
    if (!map.contains(logId.id)) {
      return ResultT<PrototypeDump>::success(map[logId.id] = PrototypeDump{});
    }
    return ResultT<PrototypeDump>::success(map[logId.id]);
  }

  std::unordered_map<LogId, PrototypeDump> map{};
};
}  // namespace

struct PrototypeConcurrencyTest : test::ReplicatedLogTest {
  PrototypeConcurrencyTest() {
    feature->registerStateType<prototype::PrototypeState>(
        "prototype-state", networkMock, storageMock);
    leader->triggerAsyncReplication();

    auto replicatedState =
        feature->createReplicatedState("prototype-state", leaderLog);
    replicatedState->start(
        std::make_unique<ReplicatedStateToken>(StateGeneration{1}));
    state = std::dynamic_pointer_cast<PrototypeLeaderState>(
        replicatedState->getLeader());
  }

  template<typename Impl, typename MockLog>
  static auto createReplicatedLogImpl(LogId id) -> std::shared_ptr<Impl> {
    auto persisted = std::make_shared<MockLog>(id);
    auto core = std::make_unique<replicated_log::LogCore>(persisted);
    auto metrics = std::make_shared<ReplicatedLogMetricsMock>();
    auto options = std::make_shared<ReplicatedLogGlobalSettings>();

    return std::make_shared<Impl>(std::move(core), metrics, options,
                                  LoggerContext(Logger::REPLICATION2));
  }

  static auto createAsyncReplicatedLog(LogId id = LogId{0})
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
    return createReplicatedLogImpl<replicated_log::ReplicatedLog,
                                   test::MockLog>(id);
  }

  static auto createLeaderWithDefaultFlags(
      std::shared_ptr<replication2::replicated_log::ReplicatedLog>& log,
      ParticipantId id, LogTerm term,
      std::vector<std::shared_ptr<AbstractFollower>> const& follower,
      std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
    auto config =
        LogConfig{writeConcern, writeConcern, follower.size() + 1, false};
    auto participants =
        std::unordered_map<ParticipantId, ParticipantFlags>{{id, {}}};
    for (auto const& participant : follower) {
      participants.emplace(participant->getParticipantId(), ParticipantFlags{});
    }
    auto participantsConfig =
        std::make_shared<ParticipantsConfig>(ParticipantsConfig{
            .generation = 1,
            .participants = std::move(participants),
        });
    return log->becomeLeader(config, std::move(id), term, follower,
                             std::move(participantsConfig),
                             std::make_shared<FakeFailureOracle>());
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
  std::shared_ptr<replication2::replicated_log::ReplicatedLog> leaderLog =
      createAsyncReplicatedLog();
  std::shared_ptr<replication2::replicated_log::ReplicatedLog> followerLog =
      createAsyncReplicatedLog();

  std::shared_ptr<LogFollower> follower =
      followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  std::shared_ptr<LogLeader> leader = createLeaderWithDefaultFlags(
      leaderLog, "leader", LogTerm{1}, {follower}, 2);

  std::shared_ptr<PrototypeLeaderState> state;

  std::shared_ptr<MockPrototypeNetworkInterface> networkMock =
      std::make_shared<MockPrototypeNetworkInterface>();
  std::shared_ptr<MockPrototypeStorageInterface> storageMock =
      std::make_shared<MockPrototypeStorageInterface>();
};

namespace {
struct WaitGroup {
  void add(std::size_t delta = 1) {
    std::unique_lock guard(mutex);
    counter += delta;
  }

  void wait() {
    std::unique_lock guard(mutex);
    cv.wait(guard, [this] { return counter == 0; });
  }

  void done() {
    std::unique_lock guard(mutex);
    counter -= 1;
    if (counter == 0) {
      cv.notify_all();
    }
  }

  std::mutex mutex;
  std::condition_variable cv;
  std::size_t counter{0};
};
}  // namespace

TEST_F(PrototypeConcurrencyTest, test_concurrent_wrires) {
  leader->waitForLeadership().get();
  const auto numKeys = 1000;
  const auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  WaitGroup wg;
  auto const runThread = [&wg, options, this](int from, int to, int delta,
                                              std::string const& myName,
                                              std::vector<LogIndex>& idxs) {
    for (int x = from; x != to; x += delta) {
      idxs[x] = state
                    ->set(
                        std::unordered_map<std::string, std::string>{
                            {std::to_string(x), myName}},
                        options)
                    .get();
    }

    wg.done();
  };

  wg.add(2);
  std::vector<LogIndex> aIdxs(numKeys + 1), bIdxs(numKeys + 1);

  std::thread a{runThread, 0, numKeys, 1, "A", std::ref(aIdxs)};
  std::thread b{runThread, numKeys, 0, -1, "B", std::ref(bIdxs)};

  wg.wait();

  auto snapshot = state->getSnapshot(LogIndex{1}).get();
  for (int x = 0; x <= numKeys; x++) {
    bool expectA = aIdxs[x] > bIdxs[x];
    auto value = snapshot->at(std::to_string(x));
    EXPECT_EQ(value, expectA ? "A" : "B")
        << "at key " << x << " A idx = " << aIdxs[x] << " B idx = " << bIdxs[x];
  }

  a.join();
  b.join();
}