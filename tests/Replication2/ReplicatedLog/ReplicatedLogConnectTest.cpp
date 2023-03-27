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

#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Replication2/Mocks/ParticipantsFactoryMock.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace arangodb::replication2::test {

struct TermBuilder {
  auto setTerm(LogTerm termNo) -> TermBuilder& {
    term.term = termNo;
    return *this;
  }

  auto setLeader(ParticipantId id, RebootId rebootId = RebootId{1})
      -> TermBuilder& {
    term.leader.emplace(std::move(id), rebootId);
    return *this;
  }

  auto setLeader(agency::ServerInstanceReference ref) -> TermBuilder& {
    term.leader.emplace(std::move(ref));
    return *this;
  }

  [[nodiscard]] auto get() const noexcept
      -> agency::LogPlanTermSpecification const& {
    return term;
  }

 private:
  agency::LogPlanTermSpecification term;
};
struct ParticipantsConfigBuilder {
  auto& setEffectiveWriteConcern(std::size_t wc) {
    result.config.effectiveWriteConcern = wc;
    return *this;
  }
  auto& setWaitForSync(bool ws) {
    result.config.waitForSync = ws;
    return *this;
  }
  auto& incGeneration(std::size_t delta = 1) {
    result.generation += delta;
    return *this;
  }
  auto& setParticipant(ParticipantId const& id, ParticipantFlags flags) {
    result.participants[id] = flags;
    return *this;
  }

  [[nodiscard]] auto get() const noexcept -> agency::ParticipantsConfig const& {
    return result;
  }

 private:
  agency::ParticipantsConfig result;
};

}  // namespace arangodb::replication2::test

namespace {

struct LogLeaderMock : replicated_log::ILogLeader {
  MOCK_METHOD(LogStatus, getStatus, (), (const, override));
  MOCK_METHOD(QuickLogStatus, getQuickStatus, (), (const, override));
  MOCK_METHOD(
      (std::tuple<std::unique_ptr<replicated_state::IStorageEngineMethods>,
                  std::unique_ptr<IReplicatedStateHandle>, DeferredAction>),
      resign, (), (override, ref(&&)));

  MOCK_METHOD(WaitForFuture, waitForLeadership, (), (override));
  MOCK_METHOD(WaitForFuture, waitFor, (LogIndex), (override));
  MOCK_METHOD(WaitForIteratorFuture, waitForIterator, (LogIndex), (override));
  MOCK_METHOD(std::unique_ptr<PersistedLogIterator>, getInternalLogIterator,
              (std::optional<LogRange> bounds), (const, override));
  MOCK_METHOD(Result, release, (LogIndex), (override));
  MOCK_METHOD(ResultT<arangodb::replication2::replicated_log::CompactionResult>,
              compact, (), (override));
  MOCK_METHOD(LogIndex, ping, (std::optional<std::string>), (override));
  MOCK_METHOD(LogIndex, insert, (LogPayload, bool), ());
  MOCK_METHOD(LogIndex, updateParticipantsConfig,
              (const std::shared_ptr<const agency::ParticipantsConfig>& config),
              (override));
};

struct LogFollowerMock : replicated_log::ILogFollower {
  MOCK_METHOD(LogStatus, getStatus, (), (const, override));
  MOCK_METHOD(QuickLogStatus, getQuickStatus, (), (const, override));
  MOCK_METHOD(
      (std::tuple<std::unique_ptr<replicated_state::IStorageEngineMethods>,
                  std::unique_ptr<IReplicatedStateHandle>, DeferredAction>),
      resign, (), (override, ref(&&)));

  MOCK_METHOD(WaitForFuture, waitFor, (LogIndex), (override));
  MOCK_METHOD(WaitForIteratorFuture, waitForIterator, (LogIndex), (override));
  MOCK_METHOD(std::unique_ptr<PersistedLogIterator>, getInternalLogIterator,
              (std::optional<LogRange> bounds), (const, override));
  MOCK_METHOD(Result, release, (LogIndex), (override));
  MOCK_METHOD(ResultT<arangodb::replication2::replicated_log::CompactionResult>,
              compact, (), (override));

  MOCK_METHOD(ParticipantId const&, getParticipantId, (),
              (const, noexcept, override));
  MOCK_METHOD(futures::Future<AppendEntriesResult>, appendEntries,
              (AppendEntriesRequest), (override));
};

struct ReplicatedStateHandleMock : IReplicatedStateHandle {
  MOCK_METHOD(std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>,
              resignCurrentState, (), (noexcept, override));
  MOCK_METHOD(void, leadershipEstablished,
              (std::unique_ptr<IReplicatedLogLeaderMethods>), (override));
  MOCK_METHOD(void, becomeFollower,
              (std::unique_ptr<IReplicatedLogFollowerMethods>), (override));
  MOCK_METHOD(void, acquireSnapshot, (ServerID leader, LogIndex, std::uint64_t),
              (noexcept, override));
  MOCK_METHOD(replicated_state::Status, getInternalStatus, (),
              (const, override));
  MOCK_METHOD(void, updateCommitIndex, (LogIndex), (noexcept, override));
};
}  // namespace

struct ReplicatedLogConnectTest : ::testing::Test {
  std::shared_ptr<test::FakeStorageEngineMethodsContext> storageContext =
      std::make_shared<test::FakeStorageEngineMethodsContext>(
          12, LogId{12}, std::make_shared<test::ThreadAsyncExecutor>());
  replicated_state::IStorageEngineMethods* methodsPtr =
      storageContext->getMethods().release();
  std::shared_ptr<test::ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<test::ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  LoggerContext loggerContext = LoggerContext(Logger::REPLICATION2);
  agency::ServerInstanceReference const myself = {"SELF", RebootId{1}};

  std::shared_ptr<test::ParticipantsFactoryMock> participantsFactoryMock =
      std::make_shared<test::ParticipantsFactoryMock>();
  std::shared_ptr<LogLeaderMock> leaderMock;
  std::shared_ptr<LogFollowerMock> followerMock;
  ReplicatedStateHandleMock* stateHandlePtr = new ReplicatedStateHandleMock();
};

TEST_F(ReplicatedLogConnectTest, construct_leader_on_connect) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
      logMetricsMock, optionsMock, participantsFactoryMock, loggerContext,
      myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              LeaderTermInfo info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.term, LogTerm{1});
            EXPECT_EQ(*info.initialConfig, config.get());
            leaderMock = std::make_shared<LogLeaderMock>();
            EXPECT_CALL(*leaderMock, waitForLeadership)
                .WillOnce([]() -> futures::Future<WaitForResult> {
                  return WaitForResult{};
                });
            return leaderMock;
          });

  log->updateConfig(term.get(), config.get(), myself);
  auto connection =
      log->connect(std::unique_ptr<IReplicatedStateHandle>{stateHandlePtr});

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, construct_leader_on_update_config) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
      logMetricsMock, optionsMock, participantsFactoryMock, loggerContext,
      myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  auto connection =
      log->connect(std::unique_ptr<IReplicatedStateHandle>{stateHandlePtr});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              LeaderTermInfo info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.term, LogTerm{1});
            EXPECT_EQ(*info.initialConfig, config.get());
            leaderMock = std::make_shared<LogLeaderMock>();
            EXPECT_CALL(*leaderMock, waitForLeadership)
                .WillOnce([]() -> futures::Future<WaitForResult> {
                  return WaitForResult{};
                });
            return leaderMock;
          });

  log->updateConfig(term.get(), config.get(), myself);

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, update_leader_to_follower) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
      logMetricsMock, optionsMock, participantsFactoryMock, loggerContext,
      myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              LeaderTermInfo info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.term, LogTerm{1});
            EXPECT_EQ(*info.initialConfig, config.get());
            leaderMock = std::make_shared<LogLeaderMock>();
            EXPECT_CALL(*leaderMock, waitForLeadership)
                .WillOnce([]() -> futures::Future<WaitForResult> {
                  return WaitForResult{};
                });
            return leaderMock;
          });
  // create initial state and connection
  auto connection =
      log->connect(std::unique_ptr<IReplicatedStateHandle>{stateHandlePtr});
  log->updateConfig(term.get(), config.get(), myself);

  // update term and make A leader
  term.setTerm(LogTerm{2}).setLeader("A");

  EXPECT_CALL(*participantsFactoryMock, constructFollower)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              FollowerTermInfo const& info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.leader, "A");
            EXPECT_EQ(info.term, LogTerm{2});
            return followerMock = std::make_shared<LogFollowerMock>();
          });

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  log->updateConfig(term.get(), config.get(), myself);
  testing::Mock::VerifyAndClearExpectations(participantsFactoryMock.get());

  EXPECT_CALL(std::move(*followerMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, update_follower_to_leader) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
      logMetricsMock, optionsMock, participantsFactoryMock, loggerContext,
      myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader("B");

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructFollower)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              FollowerTermInfo const& info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.leader, "B");
            EXPECT_EQ(info.term, LogTerm{1});
            return followerMock = std::make_shared<LogFollowerMock>();
          });
  // create initial state and connection
  auto connection =
      log->connect(std::unique_ptr<IReplicatedStateHandle>{stateHandlePtr});
  log->updateConfig(term.get(), config.get(), myself);

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              LeaderTermInfo info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            EXPECT_EQ(info.myself, myself.serverId);
            EXPECT_EQ(info.term, LogTerm{2});
            EXPECT_EQ(*info.initialConfig, config.get());
            leaderMock = std::make_shared<LogLeaderMock>();
            EXPECT_CALL(*leaderMock, waitForLeadership)
                .WillOnce([]() -> futures::Future<WaitForResult> {
                  return WaitForResult{};
                });
            return leaderMock;
          });
  EXPECT_CALL(std::move(*followerMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  // update term and make myself leader
  term.setTerm(LogTerm{2}).setLeader(myself);
  log->updateConfig(term.get(), config.get(), myself);

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, leader_on_update_config) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
      logMetricsMock, optionsMock, participantsFactoryMock, loggerContext,
      myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce(
          [&](std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
              LeaderTermInfo info, ParticipantContext context) {
            EXPECT_EQ(methods.release(), methodsPtr);
            EXPECT_EQ(context.stateHandle.release(), stateHandlePtr);
            leaderMock = std::make_shared<testing::StrictMock<LogLeaderMock>>();
            EXPECT_CALL(*leaderMock, waitForLeadership)
                .WillOnce([]() -> futures::Future<WaitForResult> {
                  return WaitForResult{};
                });
            return leaderMock;
          });
  auto connection =
      log->connect(std::unique_ptr<IReplicatedStateHandle>{stateHandlePtr});
  log->updateConfig(term.get(), config.get(), myself);

  EXPECT_CALL(*leaderMock, updateParticipantsConfig)
      .WillOnce(
          [&](std::shared_ptr<agency::ParticipantsConfig const> const& cfg) {
            EXPECT_EQ(*cfg, config.get());
            return LogIndex{1};
          });
  EXPECT_CALL(*leaderMock, waitFor(LogIndex{1}))
      .WillOnce(testing::Return(WaitForResult{}));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // This is just to make an assertion in the ReplicatedLog happy
  EXPECT_CALL(*leaderMock, getQuickStatus)
      .WillOnce([&, oldConfig = config.get()]() {
        return QuickLogStatus{
            .activeParticipantsConfig =
                std::make_shared<agency::ParticipantsConfig>(oldConfig)};
      });
#endif

  // should update leader, but not rebuild
  config.setParticipant("C", {}).incGeneration();
  log->updateConfig(term.get(), config.get(), myself);

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(
        std::unique_ptr<replicated_state::IStorageEngineMethods>(methodsPtr),
        std::unique_ptr<IReplicatedStateHandle>(stateHandlePtr),
        DeferredAction{});
  });
  connection.disconnect();
}

// TODO add test where server instance reference changed
