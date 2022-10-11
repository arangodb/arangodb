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

namespace arangodb::replication2::test {

struct TermBuilder {
  auto setTerm(LogTerm termNo) -> TermBuilder& {
    term.term = termNo;
    return *this;
  }

  auto setLeader(ParticipantId id, RebootId rebootId) -> TermBuilder& {
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
    config.config.effectiveWriteConcern = wc;
    return *this;
  }
  auto& setWaitForSync(bool ws) {
    config.config.waitForSync = ws;
    return *this;
  }
  auto& incGeneration(std::size_t delta = 1) {
    config.generation += delta;
    return *this;
  }
  auto& setParticipant(ParticipantId const& id, ParticipantFlags flags) {
    config.participants[id] = flags;
    return *this;
  }

  [[nodiscard]] auto get() const noexcept -> agency::ParticipantsConfig const& {
    return config;
  }

 private:
  agency::ParticipantsConfig config;
};

struct FakeLogFollower : replicated_log::ILogFollower {
  explicit FakeLogFollower(std::unique_ptr<replicated_log::LogCore> core)
      : core(std::move(core)) {}
  auto getParticipantId() const noexcept -> ParticipantId const& override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto appendEntries(replicated_log::AppendEntriesRequest request)
      -> futures::Future<replicated_log::AppendEntriesResult> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getStatus() const -> replicated_log::LogStatus override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getQuickStatus() const -> replicated_log::QuickLogStatus override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>,
                                 DeferredAction> override {
    return std::make_tuple(std::move(core), DeferredAction{});
  }
  auto waitFor(LogIndex index) -> WaitForFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForResign() -> futures::Future<futures::Unit> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getCommitIndex() const noexcept -> LogIndex override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto copyInMemoryLog() const -> replicated_log::InMemoryLog override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto release(LogIndex doneWithIdx) -> Result override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForLeaderAcked() -> WaitForFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getLeader() const noexcept
      -> std::optional<ParticipantId> const& override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  std::unique_ptr<replicated_log::LogCore> core;
};

struct FakeLogLeader : replicated_log::ILogLeader {
  explicit FakeLogLeader(std::unique_ptr<replicated_log::LogCore> core)
      : core(std::move(core)) {}
  auto getStatus() const -> replicated_log::LogStatus override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getQuickStatus() const -> replicated_log::QuickLogStatus override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>,
                                 DeferredAction> override {
    return std::make_tuple(std::move(core), DeferredAction{});
  }
  auto waitFor(LogIndex index) -> WaitForFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForResign() -> futures::Future<futures::Unit> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto getCommitIndex() const noexcept -> LogIndex override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto copyInMemoryLog() const -> replicated_log::InMemoryLog override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto release(LogIndex doneWithIdx) -> Result override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto insert(LogPayload payload, bool waitForSync) -> LogIndex override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto insert(LogPayload payload, bool waitForSync,
              DoNotTriggerAsyncReplication asyncReplication)
      -> LogIndex override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  void triggerAsyncReplication() override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto isLeadershipEstablished() const noexcept -> bool override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  auto waitForLeadership() -> WaitForFuture override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  std::unique_ptr<replicated_log::LogCore> core;
};

struct FakeParticipantsFactory : replicated_log::IParticipantsFactory {
  auto constructFollower(std::unique_ptr<replicated_log::LogCore> logCore,
                         replicated_log::FollowerTermInfo info,
                         replicated_log::ParticipantContext context)
      -> std::shared_ptr<replicated_log::ILogFollower> override {
    auto follower = std::make_shared<FakeLogFollower>(std::move(logCore));
    participants[info.term] = follower;
    return follower;
  }
  auto constructLeader(std::unique_ptr<replicated_log::LogCore> logCore,
                       replicated_log::LeaderTermInfo info,
                       replicated_log::ParticipantContext context)
      -> std::shared_ptr<replicated_log::ILogLeader> override {
    auto leader = std::make_shared<FakeLogLeader>(std::move(logCore));
    participants[info.term] = leader;
    return leader;
  }

  auto leaderInTerm(LogTerm term) const -> std::shared_ptr<FakeLogLeader> {
    if (auto it = participants.find(term); it != participants.end()) {
      return std::dynamic_pointer_cast<FakeLogLeader>(it->second);
    }
    return nullptr;
  }

  std::unordered_map<LogTerm, std::shared_ptr<replicated_log::ILogParticipant>>
      participants;
};

}  // namespace arangodb::replication2::test

struct ReplicatedLogTest2 : ::testing::Test {
  std::shared_ptr<test::FakeStorageEngineMethodsContext> storageContext =
      std::make_shared<test::FakeStorageEngineMethodsContext>(
          12, LogId{12}, std::make_shared<test::ThreadAsyncExecutor>());
  std::unique_ptr<replicated_state::IStorageEngineMethods> methods =
      storageContext->getMethods();
  std::unique_ptr<replicated_log::LogCore> core =
      std::make_unique<replicated_log::LogCore>(*methods);
  std::shared_ptr<ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  LoggerContext loggerContext = LoggerContext(Logger::REPLICATION2);
  agency::ServerInstanceReference const myself = {"SELF", RebootId{1}};
  std::shared_ptr<test::FakeParticipantsFactory> participantsFactory =
      std::make_shared<test::FakeParticipantsFactory>();

  test::LogEventRecorder recorder;
};

TEST_F(ReplicatedLogTest2, construct_leader_on_connect) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::move(core), logMetricsMock, optionsMock, participantsFactory,
      loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant("SELF", {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  log->updateConfig(term.get(), config.get());
  auto connection = log->connect(recorder.createHandle());

  auto leader = participantsFactory->leaderInTerm(LogTerm{1});
  ASSERT_NE(leader, nullptr);
  {
    auto participant =
        std::dynamic_pointer_cast<test::FakeLogLeader>(log->getParticipant());
    ASSERT_EQ(leader, participant);
  }
}

TEST_F(ReplicatedLogTest2, construct_leader_on_update_config) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::move(core), logMetricsMock, optionsMock, participantsFactory,
      loggerContext, myself);

  test::TermBuilder term;
  term.setTerm(LogTerm{1}).setLeader(myself);

  test::ParticipantsConfigBuilder config;
  config.setEffectiveWriteConcern(2)
      .setParticipant("SELF", {.allowedInQuorum = true})
      .setParticipant("A", {.allowedInQuorum = true})
      .setParticipant("B", {.allowedInQuorum = true});

  auto connection = log->connect(recorder.createHandle());
  log->updateConfig(term.get(), config.get());

  auto leader = participantsFactory->leaderInTerm(LogTerm{1});
  ASSERT_NE(leader, nullptr);
  {
    auto participant =
        std::dynamic_pointer_cast<test::FakeLogLeader>(log->getParticipant());
    ASSERT_EQ(leader, participant);
  }
}
