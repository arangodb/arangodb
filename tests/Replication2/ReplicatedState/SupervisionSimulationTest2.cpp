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

#include <utility>
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/Helper/AgencyLogBuilder.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/Predicates.h"

#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/Helper/ModelChecker/Predicates.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/Helper/ModelChecker/Actors.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

struct ReplicatedStateSupervisionSimulationTest2 : ::testing::Test {
  LogConfig const defaultConfig = {2, 2, 3, false};
  LogId const logId{12};

  ParticipantFlags const flagsSnapshotComplete{};
  ParticipantFlags const flagsSnapshotIncomplete{.allowedInQuorum = false,
                                                 .allowedAsLeader = false};
};

TEST_F(ReplicatedStateSupervisionSimulationTest2, check_state_and_log) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig);

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = std::nullopt,
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{},
      DBServerActor{"A"},
      DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
  };

  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;
  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2,
       check_state_and_log_with_leader) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetLeader("A")
      .setTargetConfig(defaultConfig);

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = std::nullopt,
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{},
      DBServerActor{"A"},
      DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
      MC_EVENTUALLY_ALWAYS(mcpreds::serverIsLeader("A")),
  };

  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;
  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2,
       check_state_and_log_kill_any) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig);

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = std::nullopt,
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillAnyServerActor{}, DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;
  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2,
       check_state_and_log_kill_server) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig);

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(1),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = std::nullopt,
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillLeaderActor{},  DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2, everything_ok_kill_server) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig);
  state.setPlanParticipants("A", "B", "C");
  state.setAllSnapshotsComplete();

  AgencyLogBuilder log;
  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete);
  log.setPlanLeader("A");
  log.establishLeadership();
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = log.get(),
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillLeaderActor{},  DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2, change_leader) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig)
      .setTargetLeader("C");
  state.setPlanParticipants("A", "B", "C");
  state.setAllSnapshotsComplete();

  AgencyLogBuilder log;
  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete);
  log.setPlanLeader("A");
  log.establishLeadership();
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = log.get(),
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillServerActor{"A"}, DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
      MC_EVENTUALLY_ALWAYS(mcpreds::serverIsLeader("C")),
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2,
       everything_ok_replace_server) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B")
      .setTargetVersion(20)
      .setTargetLeader("A")
      .setTargetConfig(defaultConfig);

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = std::nullopt,
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, ReplaceSpecificServerActor{"B", "C"},
      DBServerActor{"A"}, DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto eventuallyDAdded = MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(global, {
    AgencyState const& agency = global.state;
    // check that D is in plan and current
    if (!agency.replicatedState) {
      return false;
    }

    if (!agency.replicatedState->plan || !agency.replicatedState->current) {
      return false;
    }

    if (agency.replicatedState->plan->participants.size() != 2) {
      return false;
    }
    if (!agency.replicatedState->plan->participants.contains("C")) {
      return false;
    }
    auto iter = agency.replicatedState->current->participants.find("C");
    if (iter == agency.replicatedState->current->participants.end()) {
      return false;
    }

    return true;
  }));

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
      eventuallyDAdded,
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedStateSupervisionSimulationTest2,
       everything_ok_replace_leader) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig)
      .setTargetLeader("C");
  state.setPlanParticipants("A", "B");
  state.setAllSnapshotsComplete();

  AgencyLogBuilder log;
  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete);
  log.setPlanLeader("A");
  log.establishLeadership();
  log.acknowledgeTerm("A").acknowledgeTerm("B");

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto initState = AgencyState{.replicatedState = state.get(),
                               .replicatedLog = log.get(),
                               .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, ReplaceSpecificServerActor{"A", "C"},
      DBServerActor{"A"}, DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto eventuallyDAdded = MC_EVENTUALLY_ALWAYS(MC_BOOL_PRED(global, {
    AgencyState const& agency = global.state;
    // check that D is in plan and current
    if (!agency.replicatedState) {
      return false;
    }

    if (!agency.replicatedState->plan || !agency.replicatedState->current) {
      return false;
    }

    if (agency.replicatedState->plan->participants.size() != 2) {
      return false;
    }
    if (!agency.replicatedState->plan->participants.contains("C")) {
      return false;
    }
    auto iter = agency.replicatedState->current->participants.find("C");
    if (iter == agency.replicatedState->current->participants.end()) {
      return false;
    }

    return true;
  }));

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_ALWAYS(mcpreds::nonExcludedServerHasSnapshot()),
      eventuallyDAdded,
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}
