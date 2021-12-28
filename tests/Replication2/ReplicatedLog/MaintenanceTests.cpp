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

#include "Cluster/Maintenance.h"
#include "Replication2/ReplicatedLog/LogStatus.h"

using namespace arangodb;
using namespace arangodb::maintenance;
using namespace arangodb::replication2;

struct ReplicationMaintenanceTest : ::testing::Test {
  MaintenanceFeature::errors_t errors;
  std::unordered_set<DatabaseID> dirtyset;
  bool callNotify = false;
  std::vector<std::shared_ptr<ActionDescription>> actions;
};

/*
 * These tests check if the maintenance generates an action when necessary.
 */

TEST_F(ReplicationMaintenanceTest, create_replicated_log_we_are_participant) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{};
  auto const defaultConfig = LogConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              defaultConfig,
              std::nullopt,
              {
                  {ParticipantId{"A"}, {}},
                  {ParticipantId{"leader"}, {}},
              },
          },
          defaultConfig,
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
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
  auto const defaultConfig = LogConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              defaultConfig,
              std::nullopt,
              {
                  {ParticipantId{"B"}, {}},
                  {ParticipantId{"leader"}, {}},
              },
          },
          defaultConfig,
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 0);
  EXPECT_TRUE(dirtyset.find(database) == dirtyset.end());
  EXPECT_FALSE(callNotify);
}

TEST_F(ReplicationMaintenanceTest,
       create_replicated_log_we_are_not_participant_but_have_the_log) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const defaultConfig = LogConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              defaultConfig,
              std::nullopt,
              {
                  {ParticipantId{"B"}, {}},
                  {ParticipantId{"leader"}, {}},
              },
          },
          defaultConfig,
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
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
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const defaultConfig = LogConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              defaultConfig,
              std::nullopt,
              {
                  {ParticipantId{"A"}, {}},
                  {ParticipantId{"leader"}, {}},
              },
          },
          defaultConfig,
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
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
      replicated_log::QuickLogStatus{
          .role = replicated_log::ParticipantRole::kFollower,
          .term = LogTerm{4},
          .local = {}},
  }};
  auto const defaultConfig = LogConfig{};

  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {
          logId,
          agency::LogPlanTermSpecification{
              LogTerm{3},
              defaultConfig,
              std::nullopt,
              {
                  {ParticipantId{"A"}, {}},
                  {ParticipantId{"leader"}, {}},
              },
          },
          defaultConfig,
      },
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
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

  // Expect updates in case we are leader
  auto participantsConfig =
      ParticipantsConfig{1,
                         {
                             {ParticipantId{"A"}, {}},
                             {ParticipantId{"leader"}, {}},
                         }};
  auto leaderStatus = replicated_log::QuickLogStatus{
      .role = replicated_log::ParticipantRole::kLeader,
      .term = LogTerm{3},
      .local = {},
      .leadershipEstablished = true,
      .activeParticipantConfig =
          std::make_shared<ParticipantsConfig const>(participantsConfig),
      .committedParticipantConfig =
          std::make_shared<ParticipantsConfig const>(participantsConfig)};

  auto localLogs = ReplicatedLogStatusMap{
      {logId, replicated_log::QuickLogStatus{std::move(leaderStatus)}},
  };
  auto const defaultConfig = LogConfig{};

  // Modify generation to trigger an update
  participantsConfig.generation = 2;
  auto const planLogs = ReplicatedLogSpecMap{{
      logId,
      {logId,
       agency::LogPlanTermSpecification{
           LogTerm{3},
           defaultConfig,
           std::nullopt,
           {
               {ParticipantId{"A"}, {}},
               {ParticipantId{"leader"}, {}},
           },
       },
       defaultConfig, participantsConfig},
  }};

  diffReplicatedLogs(database, localLogs, planLogs, "leader", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);

  // No new updates in case we are follower
  localLogs = ReplicatedLogStatusMap{
      {logId, replicated_log::QuickLogStatus{
                  .role = replicated_log::ParticipantRole::kFollower,
                  .term = LogTerm{3},
                  .local = {}}}};

  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);
  EXPECT_EQ(actions.size(), 1);
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}

TEST_F(ReplicationMaintenanceTest, create_replicated_log_no_longer_in_plan) {
  auto const logId = LogId{12};
  auto const database = DatabaseID{"mydb"};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId, replicated_log::QuickLogStatus{
                  .role = replicated_log::ParticipantRole::kFollower,
                  .term = LogTerm{3},
                  .local = {}}}};

  auto const planLogs = ReplicatedLogSpecMap{};
  diffReplicatedLogs(database, localLogs, planLogs, "A", errors, dirtyset,
                     callNotify, actions);

  ASSERT_EQ(actions.size(), 1);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_LOG);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);
}