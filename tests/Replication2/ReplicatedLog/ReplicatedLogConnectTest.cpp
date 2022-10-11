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

using namespace arangodb;
using namespace arangodb::replication2;

namespace arangodb::replication2::test {

struct FakeFollowerFactory : replicated_log::IAbstractFollowerFactory {
  auto constructFollower(const ParticipantId& id)
      -> std::shared_ptr<replicated_log::AbstractFollower> override {
    return followers[id] =
               std::make_shared<FakeFollower>(id, "foo", LogTerm{1});
  }

  std::unordered_map<ParticipantId, std::shared_ptr<FakeFollower>> followers;
};

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
  std::shared_ptr<test::FakeFollowerFactory> followerFactory =
      std::make_shared<test::FakeFollowerFactory>();

  test::LogEventRecorder recorder;
};

TEST_F(ReplicatedLogTest2, foo_bar) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::move(core), logMetricsMock, optionsMock, followerFactory,
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
  EXPECT_EQ(recorder.events.size(), 0);
}
