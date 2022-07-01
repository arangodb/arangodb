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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "Replication2/Helper/AgencyLogBuilder.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/Predicates.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

#include "Replication2/Helper/ModelChecker/Actors.h"
#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/Helper/ModelChecker/Predicates.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;

struct ReplicatedLogSupervisionSimulationTest
    : ::testing::Test,
      model_checker::testing::TracedSeedGenerator {
  LogTargetConfig const defaultConfig = {2, 2, false};
  LogId const logId{23};
  ParticipantFlags const defaultFlags{};
};

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_created) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{},
      DBServerActor{"A"},
      DBServerActor{"B"},
      DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
  };
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_leader_fails) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillLeaderActor{},  DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),

      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
  };
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_any_fails) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillAnyServerActor{}, DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),

      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
  };
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest,
       check_participant_added_created) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags)
      .setTargetParticipant("F", defaultFlags);

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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  // TODO once actor that adds a participant
  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, AddServerActor{"D"}, RemoveServerActor{"F"},
      DBServerActor{"A"}, DBServerActor{"B"},  DBServerActor{"C"},
      DBServerActor{"D"}};

  // TODO predicate that checks participant is there
  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isParticipantPlanned("D")),
      MC_EVENTUALLY_ALWAYS(mcpreds::isParticipantCurrent("D")),
      MC_ALWAYS(mcpreds::isParticipantNotPlanned("E")),
      MC_EVENTUALLY_ALWAYS(mcpreds::isParticipantNotPlanned("F")),
  };
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);
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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, KillLeaderActor{}, DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"}};

  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
  };
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_set_leader) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);
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

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, SetLeaderActor{"C"}, DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"}};

  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_EVENTUALLY_ALWAYS(mcpreds::serverIsLeader("C"))};
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_change_config) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);

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
  health._health.emplace(
      "D", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "E", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "F", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "G", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{},          KillLeaderActor{},
      SetLeaderActor{"C"},         DBServerActor{"A"},
      DBServerActor{"B"},          DBServerActor{"C"},
      DBServerActor{"D"},          AddServerActor{"D"},
      RemoveServerActor{"A"},      SetWriteConcernActor{1},
      SetSoftWriteConcernActor{3}, SetBothWriteConcernActor{2, 3}};

  auto allTests = model_checker::combined{
      MC_ALWAYS(
          mcpreds::isAssumedWriteConcernLessThanWriteConcernUsedForCommit()),
      MC_ALWAYS(
          mcpreds::
              isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_EVENTUALLY_ALWAYS(mcpreds::serverIsLeader("C"))};
  // Unfortunately the deterministic checker takes too long
  //  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
  //                                              AgencyState,
  //                                              AgencyTransition>;
  using Engine = model_checker::ActorEngine<model_checker::RandomEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result =
      Engine::run(driver, allTests, initState,
                  {.iterations = 20000, .seed = this->seed(ADB_HERE)});
  EXPECT_FALSE(result.failed) << *result.failed;
  //  std::cout << result.stats << std::endl;
}

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log_replace_leader) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig(2, 2, true))
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);
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
  health._health.emplace(
      "D", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto initState =
      AgencyState{.replicatedLog = log.get(), .health = std::move(health)};

  auto driver = model_checker::ActorDriver{
      SupervisionActor{}, ReplaceSpecificLogServerActor{"A", "D"},
      DBServerActor{"A"}, DBServerActor{"B"},
      DBServerActor{"C"}, DBServerActor{"D"}};

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
      MC_EVENTUALLY_ALWAYS(mcpreds::isParticipantNotPlanned("A"))};
  using Engine = model_checker::ActorEngine<model_checker::DFSEnumerator,
                                            AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}
