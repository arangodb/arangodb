////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gtest/gtest.h>

#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/Mocks/ReplicatedStateHandleMock.h"
#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Replication2/Mocks/SchedulerMocks.h"
#include "Replication2/Mocks/RebootIdCacheMock.h"
#include "Replication2/Mocks/FakeFollowerFactory.h"

#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/Components/LogFollower.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::test {

struct LogArguments {
  LogRange initialLogRange = {};
  // set persistedMetadata to std::nullopt to simulate a read failure.
  // `.stateId` will be set by makeLogWithFakes(), but is of no consequence
  // anyway.
  std::optional<storage::PersistedStateInfo> persistedMetadata =
      storage::PersistedStateInfo{};
};
struct ConfigArguments {
  LogTerm term = LogTerm{1};
  std::size_t writeConcern = 1;
  bool waitForSync = false;
};
struct LogConfig {
  agency::LogPlanTermSpecification termSpec;
  agency::ParticipantsConfig participantsConfig;
};

struct LogWithFakes {
  LogWithFakes(LogId logId, agency::ServerInstanceReference serverId,
               LoggerContext loggerContext,
               std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics,
               LogArguments fakeArguments)
      : loggerContext(std::move(loggerContext)),
        logId(logId),
        serverInstance(std::move(serverId)),
        logMetrics(std::move(logMetrics)),
        storageContext(
            std::make_shared<storage::test::FakeStorageEngineMethodsContext>(
                12, gid.id, executor, fakeArguments.initialLogRange,
                std::move(fakeArguments.persistedMetadata))) {}

  [[nodiscard]] auto updateConfig(LogConfig conf) {
    return log->updateConfig(std::move(conf.termSpec),
                             std::move(conf.participantsConfig),
                             serverInstance);
  }
  [[nodiscard]] auto getAsLeader() {
    auto const leader = std::dynamic_pointer_cast<replicated_log::LogLeader>(
        log->getParticipant());
    EXPECT_NE(leader, nullptr);
    return leader;
  }
  [[nodiscard]] auto getAsFollower() {
    auto const follower =
        std::dynamic_pointer_cast<replicated_log::LogFollowerImpl>(
            log->getParticipant());
    EXPECT_NE(follower, nullptr);
    return follower;
  }
  auto runScheduler() {
    auto iters = std::size_t{0};
    while (logScheduler->hasWork()) {
      ++iters;
      logScheduler->runOnce();
    }
    return iters;
  }

  LoggerContext loggerContext;
  LogId const logId;
  agency::ServerInstanceReference const serverInstance;
  std::shared_ptr<replicated_log::ReplicatedLogMetrics> logMetrics;

  GlobalLogIdentifier gid = GlobalLogIdentifier("db", logId);

  std::shared_ptr<storage::rocksdb::test::DelayedExecutor> executor =
      std::make_shared<storage::rocksdb::test::DelayedExecutor>();
  // Note that this purposefully does not initialize the PersistedStateInfo that
  // is returned by the StorageEngineMethods. readMetadata() will return a
  // document not found error unless you initialize it in your test.
  std::shared_ptr<storage::test::FakeStorageEngineMethodsContext>
      storageContext;
  storage::IStorageEngineMethods* methodsPtr =
      storageContext->getMethods().release();
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();

  std::shared_ptr<DelayedScheduler> logScheduler =
      std::make_shared<DelayedScheduler>();
  std::shared_ptr<test::RebootIdCacheMock> rebootIdCache =
      std::make_shared<test::RebootIdCacheMock>();
  std::shared_ptr<test::FakeFollowerFactory> fakeFollowerFactory =
      std::make_shared<FakeFollowerFactory>();
  std::shared_ptr<replicated_log::DefaultParticipantsFactory>
      participantsFactory =
          std::make_shared<replicated_log::DefaultParticipantsFactory>(
              fakeFollowerFactory, logScheduler, rebootIdCache);

  std::shared_ptr<replicated_log::ReplicatedLog> log =
      std::make_shared<replicated_log::ReplicatedLog>(
          std::unique_ptr<storage::IStorageEngineMethods>{methodsPtr},
          logMetrics, optionsMock, participantsFactory, loggerContext,
          serverInstance);

  test::ReplicatedStateHandleMock* stateHandleMock =
      new test::ReplicatedStateHandleMock();
  replicated_log::ReplicatedLogConnection connection = log->connect(
      std::unique_ptr<replicated_log::IReplicatedStateHandle>(stateHandleMock));
};

struct ReplicatedLogTest : ::testing::Test {
  testing::TestInfo const* const testInfo =
      testing::UnitTest::GetInstance()->current_test_info();
  constexpr static char gtestStr[] = "gtest";
  LoggerContext loggerContext =
      LoggerContext(Logger::REPLICATION2)
          .with<gtestStr>(fmt::format("{}.{}", testInfo->test_suite_name(),
                                      testInfo->test_case_name()));

  std::shared_ptr<test::ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<test::ReplicatedLogMetricsMock>();

  std::uint64_t nextId = 1;

  auto makeLogWithFakes(LogArguments fakeArguments) {
    auto const id = nextId++;
    auto logId = LogId{id};
    auto serverId = agency::ServerInstanceReference{fmt::format("dbs{:02}", id),
                                                    RebootId{1}};
    if (fakeArguments.persistedMetadata.has_value()) {
      fakeArguments.persistedMetadata->stateId = logId;
    }
    return LogWithFakes(logId, serverId, loggerContext, logMetricsMock,
                        std::move(fakeArguments));
  }

  auto makeConfig(LogWithFakes& leader,
                  std::vector<std::reference_wrapper<LogWithFakes>> follower,
                  ConfigArguments configArguments) {
    auto logConfig = agency::LogPlanConfig{};
    auto participants = agency::ParticipantsFlagsMap{};
    std::transform(
        follower.begin(), follower.end(),
        std::inserter(participants, participants.end()), [&](LogWithFakes& f) {
          return std::pair(f.serverInstance.serverId, ParticipantFlags{});
        });
    auto participantsConfig = agency::ParticipantsConfig{
        .participants = participants, .config = logConfig};
    auto logSpec = agency::LogPlanTermSpecification{configArguments.term,
                                                    leader.serverInstance};
    return LogConfig{.termSpec = std::move(logSpec),
                     .participantsConfig = std::move(participantsConfig)};
  }
};

}  // namespace arangodb::replication2::test
