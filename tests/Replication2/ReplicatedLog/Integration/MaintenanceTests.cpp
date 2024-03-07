////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Cluster/Maintenance.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

using namespace arangodb;
using namespace arangodb::maintenance;
using namespace arangodb::replication2;

struct ReplicationMaintenanceTest : ::testing::Test {
  MaintenanceFeature::errors_t errors;
  containers::FlatHashSet<DatabaseID> dirtyset;
  bool callNotify = false;
  std::vector<std::shared_ptr<ActionDescription>> actions;
  // Note that diffReplicatedLogs() in Maintenance.cpp looks in
  // ServerState::instance()->getRebootId() for comparison. We assume that this
  // is 1 in the tests.
  // The serverId is currently unused in the tests.
  agency::ServerInstanceReference myself{{}, RebootId{1}};
};

/*
 * These tests check if the maintenance generates an action when necessary.
 */

TEST_F(ReplicationMaintenanceTest, create_replicated_log_we_are_participant) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{};
  auto const defaultConfig = agency::LogPlanConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {logId,
       agency::LogPlanTermSpecification{
           LogTerm{3},
           std::nullopt,
       },
       agency::ParticipantsConfig{.generation = 0,
                                  .participants =
                                      {
                                          {ParticipantId{"A"}, {}},
                                          {ParticipantId{"leader"}, {}},
                                      },
                                  .config = defaultConfig}},
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest,
       create_replicated_log_we_are_not_participant) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{};
  auto const defaultConfig = agency::LogPlanConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              std::nullopt,
          },
          agency::ParticipantsConfig{
              .generation = 0,
              .participants =
                  {
                      {ParticipantId{"B"}, {}},
                      {ParticipantId{"leader"}, {}},
                  },
              .config = defaultConfig,
          },
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 0U);
  EXPECT_TRUE(dirtyset.find(database) == dirtyset.end());
  EXPECT_FALSE(callNotify);
}

TEST_F(ReplicationMaintenanceTest,
       create_replicated_log_we_are_not_participant_but_have_the_log) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replication2::maintenance::LogStatus{
           replicated_log::QuickLogStatus{
               replicated_log::ParticipantRole::kUnconfigured},
           myself}},
  };
  auto const defaultConfig = agency::LogPlanConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              std::nullopt,
          },
          agency::ParticipantsConfig{
              .generation = 0,
              .participants =
                  {
                      {ParticipantId{"B"}, {}},
                      {ParticipantId{"leader"}, {}},
                  },
              .config = defaultConfig,
          },
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest, create_replicated_log_detect_unconfigured) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       arangodb::replication2::maintenance::LogStatus{
           replicated_log::QuickLogStatus{
               replicated_log::ParticipantRole::kUnconfigured},
           myself}},
  };
  auto const defaultConfig = agency::LogPlanConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              std::nullopt,
          },
          agency::ParticipantsConfig{
              .generation = 0,
              .participants =
                  {
                      {ParticipantId{"A"}, {}},
                      {ParticipantId{"leader"}, {}},
                  },
              .config = defaultConfig,
          },
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest, create_replicated_log_detect_wrong_term) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{{
      logId,
      replication2::maintenance::LogStatus{
          replicated_log::QuickLogStatus{
              .role = replicated_log::ParticipantRole::kFollower,
              .term = LogTerm{4},
              .local = {}},
          myself},
  }};
  auto const defaultConfig = agency::LogPlanConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              std::nullopt,
          },
          agency::ParticipantsConfig{
              .generation = 0,
              .participants =
                  {
                      {ParticipantId{"A"}, {}},
                      {ParticipantId{"leader"}, {}},
                  },
              .config = defaultConfig,
          },
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest,
       create_replicated_log_detect_wrong_generation) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const defaultConfig = agency::LogPlanConfig{};

  // Expect updates in case we are leader
  auto participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants =
                                     {
                                         {ParticipantId{"A"}, {}},
                                         {ParticipantId{"leader"}, {}},
                                     },
                                 .config = defaultConfig};
  auto leaderStatus = replicated_log::QuickLogStatus{
      .role = replicated_log::ParticipantRole::kLeader,
      .term = LogTerm{3},
      .local = {},
      .leadershipEstablished = true,
      .activeParticipantsConfig =
          std::make_shared<agency::ParticipantsConfig const>(
              participantsConfig),
      .committedParticipantsConfig =
          std::make_shared<agency::ParticipantsConfig const>(
              participantsConfig)};

  auto localLogs = ReplicatedLogStatusMap{
      {logId,
       replication2::maintenance::LogStatus{
           replicated_log::QuickLogStatus{std::move(leaderStatus)}, myself}},
  };

  // Modify generation to trigger an update
  participantsConfig.generation = 2;
  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {logId,
       agency::LogPlanTermSpecification{
           LogTerm{3},
           std::nullopt,
       },
       participantsConfig},
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "leader", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);

  // No new updates in case we are follower
  localLogs = ReplicatedLogStatusMap{
      {logId, replication2::maintenance::LogStatus{
                  replicated_log::QuickLogStatus{
                      .role = replicated_log::ParticipantRole::kFollower,
                      .term = LogTerm{3},
                      .local = {}},
                  myself}}};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);
  EXPECT_EQ(actions.size(), 1U);
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest, create_replicated_log_no_longer_in_plan) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId, replication2::maintenance::LogStatus{
                  replicated_log::QuickLogStatus{
                      .role = replicated_log::ParticipantRole::kFollower,
                      .term = LogTerm{3},
                      .local = {}},
                  myself}}};

  auto const planLogs = ReplicatedLogSpecMap{};
  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1U);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}
