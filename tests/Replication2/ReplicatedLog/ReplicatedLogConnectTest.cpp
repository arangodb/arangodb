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

#include "Basics/application-exit.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Replication2/Mocks/FakeFollower.h"
#include "Replication2/Mocks/LogEventRecorder.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"

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

namespace arangodb::replication2::replicated_log {
struct ParticipantsFactoryMock : IParticipantsFactory {
  MOCK_METHOD(std::shared_ptr<ILogFollower>, constructFollower,
              (std::unique_ptr<LogCore> logCore, FollowerTermInfo info,
               ParticipantContext context),
              (override));

  MOCK_METHOD(std::shared_ptr<ILogLeader>, constructLeader,
              (std::unique_ptr<LogCore> logCore, LeaderTermInfo info,
               ParticipantContext context),
              (override));
};

struct LogLeaderMock : replicated_log::ILogLeader {
  MOCK_METHOD(LogStatus, getStatus, (), (const, override));
  MOCK_METHOD(QuickLogStatus, getQuickStatus, (), (const, override));
  MOCK_METHOD((std::tuple<std::unique_ptr<LogCore>, DeferredAction>), resign,
              (), (override, ref(&&)));

  MOCK_METHOD(WaitForFuture, waitFor, (LogIndex), (override));
  MOCK_METHOD(WaitForIteratorFuture, waitForIterator, (LogIndex), (override));
  MOCK_METHOD(InMemoryLog, copyInMemoryLog, (), (const, override));
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
      (std::tuple<std::unique_ptr<replicated_log::LogCore>, DeferredAction>),
      resign, (), (override, ref(&&)));

  MOCK_METHOD(WaitForFuture, waitFor, (LogIndex), (override));
  MOCK_METHOD(WaitForIteratorFuture, waitForIterator, (LogIndex), (override));
  MOCK_METHOD(InMemoryLog, copyInMemoryLog, (), (const, override));
  MOCK_METHOD(Result, release, (LogIndex), (override));
  MOCK_METHOD(ResultT<arangodb::replication2::replicated_log::CompactionResult>,
              compact, (), (override));

  MOCK_METHOD(ParticipantId const&, getParticipantId, (), (const, noexcept));
  MOCK_METHOD(futures::Future<AppendEntriesResult>, appendEntries,
              (AppendEntriesRequest), (override));
};

}  // namespace arangodb::replication2::replicated_log

struct ReplicatedLogConnectTest : ::testing::Test {
  std::shared_ptr<test::FakeStorageEngineMethodsContext> storageContext =
      std::make_shared<test::FakeStorageEngineMethodsContext>(
          12, LogId{12}, std::make_shared<test::ThreadAsyncExecutor>());
  std::unique_ptr<replicated_state::IStorageEngineMethods> methods =
      storageContext->getMethods();
  replicated_log::LogCore* core = new replicated_log::LogCore(*methods);
  std::shared_ptr<ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  LoggerContext loggerContext = LoggerContext(Logger::REPLICATION2);
  agency::ServerInstanceReference const myself = {"SELF", RebootId{1}};

  std::shared_ptr<ParticipantsFactoryMock> participantsFactoryMock =
      std::make_shared<ParticipantsFactoryMock>();
  std::shared_ptr<LogLeaderMock> leaderMock;
  std::shared_ptr<LogFollowerMock> followerMock;

  test::LogEventRecorder recorder;
};

TEST_F(ReplicatedLogConnectTest, construct_leader_on_connect) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_log::LogCore>{core}, logMetricsMock,
      optionsMock, participantsFactoryMock, loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    LeaderTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_EQ(logCore.release(), core);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.term, LogTerm{1});
        EXPECT_EQ(*info.initialConfig, config.get());
        return leaderMock = std::make_shared<LogLeaderMock>();
      });

  log->updateConfig(term.get(), config.get());
  auto connection = log->connect(recorder.createHandle());

  EXPECT_CALL(std::move(*leaderMock), resign).Times(1);
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, construct_leader_on_update_config) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_log::LogCore>{core}, logMetricsMock,
      optionsMock, participantsFactoryMock, loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  auto connection = log->connect(recorder.createHandle());

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    LeaderTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_EQ(logCore.release(), core);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.term, LogTerm{1});
        EXPECT_EQ(*info.initialConfig, config.get());
        return leaderMock = std::make_shared<LogLeaderMock>();
      });

  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(std::move(*leaderMock), resign).Times(1);
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, update_leader_to_follower) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_log::LogCore>{core}, logMetricsMock,
      optionsMock, participantsFactoryMock, loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    LeaderTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_EQ(logCore.release(), core);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.term, LogTerm{1});
        EXPECT_EQ(*info.initialConfig, config.get());
        return leaderMock = std::make_shared<LogLeaderMock>();
      });
  // create initial state and connection
  auto connection = log->connect(recorder.createHandle());
  log->updateConfig(term.get(), config.get());

  // update term and make A leader
  term.setTerm(LogTerm{2}).setLeader("A");

  EXPECT_CALL(*participantsFactoryMock, constructFollower)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    FollowerTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_NE(logCore, nullptr);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.leader, "A");
        EXPECT_EQ(info.term, LogTerm{2});
        return followerMock = std::make_shared<LogFollowerMock>();
      });

  EXPECT_CALL(std::move(*leaderMock), resign).WillOnce([&] {
    return std::make_tuple(std::unique_ptr<replicated_log::LogCore>(core),
                           DeferredAction{});
  });
  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(std::move(*followerMock), resign);
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, update_follower_to_leader) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_log::LogCore>{core}, logMetricsMock,
      optionsMock, participantsFactoryMock, loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader("B");

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructFollower)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    FollowerTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_NE(logCore, nullptr);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.leader, "B");
        EXPECT_EQ(info.term, LogTerm{1});
        return followerMock = std::make_shared<LogFollowerMock>();
      });
  // create initial state and connection
  auto connection = log->connect(recorder.createHandle());
  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    LeaderTermInfo const& info,
                    ParticipantContext const& context) {
        EXPECT_EQ(logCore.release(), core);
        EXPECT_EQ(info.myself, myself.serverId);
        EXPECT_EQ(info.term, LogTerm{2});
        EXPECT_EQ(*info.initialConfig, config.get());
        return leaderMock = std::make_shared<LogLeaderMock>();
      });
  EXPECT_CALL(std::move(*followerMock), resign).WillOnce([&] {
    return std::make_tuple(std::unique_ptr<replicated_log::LogCore>(core),
                           DeferredAction{});
  });
  // update term and make myself leader
  term.setTerm(LogTerm{2}).setLeader(myself);
  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(std::move(*leaderMock), resign);
  connection.disconnect();
}

TEST_F(ReplicatedLogConnectTest, leader_on_update_config) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::unique_ptr<replicated_log::LogCore>{core}, logMetricsMock,
      optionsMock, participantsFactoryMock, loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant(myself.serverId, {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  EXPECT_CALL(*participantsFactoryMock, constructLeader)
      .WillOnce([&](std::unique_ptr<LogCore> logCore,
                    LeaderTermInfo const& info,
                    ParticipantContext const& context) {
        return leaderMock =
                   std::make_shared<testing::StrictMock<LogLeaderMock>>();
      });
  auto connection = log->connect(recorder.createHandle());
  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(*leaderMock, updateParticipantsConfig)
      .WillOnce(
          [&](std::shared_ptr<agency::ParticipantsConfig const> const& cfg) {
            EXPECT_EQ(*cfg, config.get());
            return LogIndex{1};
          });
  EXPECT_CALL(*leaderMock, waitFor(LogIndex{1}))
      .WillOnce(testing::Return(WaitForResult{}));

  // should update leader, but not rebuild
  config.setParticipant("C", {}).incGeneration();
  log->updateConfig(term.get(), config.get());

  EXPECT_CALL(std::move(*leaderMock), resign);
  connection.disconnect();
}
