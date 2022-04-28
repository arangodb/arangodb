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

#include "Replication2/Helper/AgencyLogBuilder.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/Predicates.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include <utility>

#include "Replication2/Helper/ModelChecker/Actors.h"
#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/Helper/ModelChecker/Predicates.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;

struct ReplicatedLogSupervisionSimulationTest : ::testing::Test {
  LogConfig const defaultConfig = {2, 2, 3, false};
  LogId const logId{23};
  ParticipantFlags const defaultFlags{};
};

TEST_F(ReplicatedLogSupervisionSimulationTest, check_log) {
  AgencyLogBuilder log;
  log.setId(logId)
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
      SupervisionActor{}, KillLeaderActor{},  DBServerActor{"A"},
      DBServerActor{"B"}, DBServerActor{"C"},
  };

  auto allTests = model_checker::combined{
      MC_EVENTUALLY_ALWAYS(mcpreds::isLeaderHealth()),
  };
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, allTests, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}
