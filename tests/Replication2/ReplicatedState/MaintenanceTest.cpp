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
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::maintenance;
using namespace arangodb::replication2;

struct ReplicatedStateMaintenanceTest : ::testing::Test {
  MaintenanceFeature::errors_t errors;
  containers::FlatHashSet<DatabaseID> dirtyset;
  bool callNotify = false;
  std::vector<std::shared_ptr<ActionDescription>> actions;
  DatabaseID const database{"mydb"};
  ParticipantId const serverId{"MyServerId"};
  LogId const logId{12};
};

namespace {
template<typename T>
auto decodeFromString(std::string_view src) -> std::optional<T> {
  auto buffer = arangodb::basics::StringUtils::decodeBase64(src);
  auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(buffer.c_str()));
  if (!slice.isNone()) {
    return velocypack::deserialize<T>(slice);
  }

  return std::nullopt;
}
}  // namespace

TEST_F(ReplicatedStateMaintenanceTest, create_state_test_without_local_log) {
  /*
   * Test that the maintenance waits for the replicated log to be present first
   * before creating a replicated state.
   */
  auto const participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants = {
                                     {serverId, {}},
                                     {"otherServer", {}},
                                 }};
  auto const localLogs = ReplicatedLogStatusMap{};
  auto const localStates = ReplicatedStateStatusMap{};
  auto const planLogs = ReplicatedLogSpecMap{
      {logId,
       agency::LogPlanSpecification{logId, std::nullopt, participantsConfig}},
  };
  auto const planStates = ReplicatedStateSpecMap{
      {logId, replicated_state::agency::Plan{
                  .id = logId,
                  .generation = replicated_state::StateGeneration{1},
                  .properties = {},
                  .participants = {
                      {serverId, {}},
                      {"otherServer", {}},
                  }}}};
  auto const currentStates = ReplicatedStateCurrentMap{};

  maintenance::diffReplicatedStates(database, localLogs, localStates, planLogs,
                                    planStates, currentStates, serverId, errors,
                                    dirtyset, callNotify, actions);
  ASSERT_EQ(actions.size(), 0);
}

TEST_F(ReplicatedStateMaintenanceTest, create_state_test_with_local_log) {
  /*
   * Test that the maintenance create an action to create the replicated state.
   */
  auto const participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants = {
                                     {serverId, {}},
                                     {"otherServer", {}},
                                 }};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const localStates = ReplicatedStateStatusMap{};
  auto const planLogs = ReplicatedLogSpecMap{
      {logId,
       agency::LogPlanSpecification{logId, std::nullopt, participantsConfig}},
  };
  auto const planStates = ReplicatedStateSpecMap{
      {logId,
       replicated_state::agency::Plan{
           .id = logId,
           .generation = replicated_state::StateGeneration{1},
           .properties = {},
           .participants = {
               {serverId, {.generation = replicated_state::StateGeneration{1}}},
               {"otherServer",
                {.generation = replicated_state::StateGeneration{1}}},
           }}}};
  auto const currentStates = ReplicatedStateCurrentMap{};

  maintenance::diffReplicatedStates(database, localLogs, localStates, planLogs,
                                    planStates, currentStates, serverId, errors,
                                    dirtyset, callNotify, actions);
  ASSERT_EQ(actions.size(), 1);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_STATE);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);

  auto current = decodeFromString<replicated_state::agency::Current>(
      action->get(REPLICATED_STATE_CURRENT));
  EXPECT_EQ(current, std::nullopt);
}

TEST_F(ReplicatedStateMaintenanceTest,
       create_state_test_with_local_log_and_current_entry) {
  /*
   * Test that the maintenance creates a replicated state and forwards the
   * current entry into the action.
   */
  auto const participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants = {
                                     {serverId, {}},
                                     {"otherServer", {}},
                                 }};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const localStates = ReplicatedStateStatusMap{};
  auto const planLogs = ReplicatedLogSpecMap{
      {logId,
       agency::LogPlanSpecification{logId, std::nullopt, participantsConfig}},
  };
  auto const planStates = ReplicatedStateSpecMap{
      {logId,
       replicated_state::agency::Plan{
           .id = logId,
           .generation = replicated_state::StateGeneration{1},
           .properties = {},
           .participants = {
               {serverId, {.generation = replicated_state::StateGeneration{1}}},
               {"otherServer",
                {.generation = replicated_state::StateGeneration{1}}},
           }}}};
  auto const currentStates = ReplicatedStateCurrentMap{{
      logId,
      replicated_state::agency::Current{
          .participants =
              {
                  {serverId,
                   {.generation = replicated_state::StateGeneration{0},
                    .snapshot = {}}},
              },
          .supervision = {}},
  }};

  maintenance::diffReplicatedStates(database, localLogs, localStates, planLogs,
                                    planStates, currentStates, serverId, errors,
                                    dirtyset, callNotify, actions);
  ASSERT_EQ(actions.size(), 1);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_STATE);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);

  auto current = decodeFromString<replicated_state::agency::Current>(
      action->get(REPLICATED_STATE_CURRENT));
  EXPECT_EQ(current, currentStates.at(logId));
}

TEST_F(ReplicatedStateMaintenanceTest, do_nothing_if_stable) {
  /*
   * Check that if the configuration is stable, nothing happens.
   */
  auto const participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants = {
                                     {serverId, {}},
                                     {"otherServer", {}},
                                 }};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const localStates = ReplicatedStateStatusMap{
      {logId,
       {{replicated_state::UnconfiguredStatus{
           .generation = replicated_state::StateGeneration{1},
           .snapshot = {}}}}},
  };
  auto const planLogs = ReplicatedLogSpecMap{
      {logId,
       agency::LogPlanSpecification{logId, std::nullopt, participantsConfig}},
  };
  auto const planStates = ReplicatedStateSpecMap{
      {logId,
       replicated_state::agency::Plan{
           .id = logId,
           .generation = replicated_state::StateGeneration{1},
           .properties = {},
           .participants = {
               {serverId, {.generation = replicated_state::StateGeneration{1}}},
               {"otherServer",
                {.generation = replicated_state::StateGeneration{1}}},
           }}}};
  auto const currentStates = ReplicatedStateCurrentMap{{
      logId,
      replicated_state::agency::Current{
          .participants =
              {
                  {serverId,
                   {.generation = replicated_state::StateGeneration{0},
                    .snapshot = {}}},
              },
          .supervision = {}},
  }};

  maintenance::diffReplicatedStates(database, localLogs, localStates, planLogs,
                                    planStates, currentStates, serverId, errors,
                                    dirtyset, callNotify, actions);
  ASSERT_EQ(actions.size(), 0);
}

TEST_F(ReplicatedStateMaintenanceTest, check_resync_if_generation_changes) {
  /*
   * Check that the maintenance triggers a resync if the generation changes.
   */
  auto const participantsConfig =
      agency::ParticipantsConfig{.generation = 1,
                                 .participants = {
                                     {serverId, {}},
                                     {"otherServer", {}},
                                 }};
  auto const localLogs = ReplicatedLogStatusMap{
      {logId,
       replicated_log::QuickLogStatus{
           replicated_log::ParticipantRole::kUnconfigured}},
  };
  auto const localStates = ReplicatedStateStatusMap{
      {logId,
       {{replicated_state::UnconfiguredStatus{
           .generation = replicated_state::StateGeneration{0},
           .snapshot = {}}}}},
  };
  auto const planLogs = ReplicatedLogSpecMap{
      {logId,
       agency::LogPlanSpecification{logId, std::nullopt, participantsConfig}},
  };
  auto const planStates = ReplicatedStateSpecMap{
      {logId,
       replicated_state::agency::Plan{
           .id = logId,
           .generation = replicated_state::StateGeneration{1},
           .properties = {},
           .participants = {
               {serverId, {.generation = replicated_state::StateGeneration{1}}},
               {"otherServer",
                {.generation = replicated_state::StateGeneration{1}}},
           }}}};
  auto const currentStates = ReplicatedStateCurrentMap{{
      logId,
      replicated_state::agency::Current{
          .participants =
              {
                  {serverId,
                   {.generation = replicated_state::StateGeneration{0},
                    .snapshot = {}}},
              },
          .supervision = {}},
  }};

  maintenance::diffReplicatedStates(database, localLogs, localStates, planLogs,
                                    planStates, currentStates, serverId, errors,
                                    dirtyset, callNotify, actions);
  ASSERT_EQ(actions.size(), 1);
  auto const& action = actions.front();
  EXPECT_EQ(action->get(NAME), UPDATE_REPLICATED_STATE);
  EXPECT_EQ(action->get(DATABASE), database);
  EXPECT_EQ(action->get(REPLICATED_LOG_ID), to_string(logId));
  EXPECT_TRUE(dirtyset.find(database) != dirtyset.end());
  EXPECT_TRUE(callNotify);

  auto current = decodeFromString<replicated_state::agency::Current>(
      action->get(REPLICATED_STATE_CURRENT));
  EXPECT_EQ(current, std::nullopt);  // Current not needed.
}
