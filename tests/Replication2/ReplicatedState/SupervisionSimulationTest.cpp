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
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/Supervision.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/Helper/AgencyLogBuilder.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

namespace {
struct AgencyState {
  RSA::State replicatedState;
  std::optional<RLA::Log> replicatedLog;
};

struct ActorState {
  std::shared_ptr<AgencyState> _localAgency;
};

struct SimulationState {
  std::shared_ptr<AgencyState> _agency;
  std::vector<ActorState> _actors;
  std::shared_ptr<SimulationState const> _previous;
};

struct Action {
  virtual ~Action() = default;
  virtual void apply(std::shared_ptr<AgencyState>) = 0;
};

struct SupervisionStateAction final : Action {
  void apply(std::shared_ptr<AgencyState> agency) override {
    auto actionCtx =
        executeAction(agency->replicatedState, agency->replicatedLog, _action);
    if (actionCtx.hasModificationFor<RLA::LogTarget>()) {
      if (!agency->replicatedLog) {
        agency->replicatedLog.emplace();
      }
      agency->replicatedLog->target = actionCtx.getValue<RLA::LogTarget>();
    }
    if (actionCtx.hasModificationFor<RSA::Plan>()) {
      agency->replicatedState.plan = actionCtx.getValue<RSA::Plan>();
    }
    if (actionCtx.hasModificationFor<RSA::Current::Supervision>()) {
      if (!agency->replicatedState.current) {
        agency->replicatedState.current.emplace();
      }
      agency->replicatedState.current->supervision =
          actionCtx.getValue<RSA::Current::Supervision>();
    }
  }

  explicit SupervisionStateAction(replicated_state::Action action)
      : _action(std::move(action)) {}

  replicated_state::Action _action;
};

struct Actor {
  virtual ~Actor() = default;
  virtual auto step(std::shared_ptr<AgencyState const> const&) const
      -> std::unique_ptr<Action> = 0;
};

struct SupervisionStateActor final : Actor {
  auto step(std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<Action> override {
    replicated_state::SupervisionContext ctx;
    ctx.enableErrorReporting();
    replicated_state::checkReplicatedState(ctx, agency->replicatedLog,
                                           agency->replicatedState);
    auto action = ctx.getAction();
    if (std::holds_alternative<EmptyAction>(action)) {
      return nullptr;
    }
    return std::make_unique<SupervisionStateAction>(std::move(action));
  }
};
struct DBServerActor final : Actor {
  explicit DBServerActor(ParticipantId name) : name(std::move(name)) {}

  auto step(std::shared_ptr<AgencyState const> const&) const
      -> std::unique_ptr<Action> override {
    return nullptr;
  }

  ParticipantId name;
};

}  // namespace

struct ReplicatedStateSupervisionSimulationTest : ::testing::Test {
  std::deque<std::shared_ptr<SimulationState>> activeStates;
  std::vector<std::unique_ptr<Actor>> actors;

  void createLoadStep(std::shared_ptr<SimulationState const> const& sim,
                      std::size_t actorIdx) {
    // first check if there is something new
    auto const& actor = sim->_actors[actorIdx];
    if (actor._localAgency != sim->_agency) {
      auto newSim = std::make_shared<SimulationState>(*sim);
      newSim->_previous = sim;
      newSim->_actors[actorIdx]._localAgency = sim->_agency;
      activeStates.push_back(newSim);
    }
  }

  void createRunStep(std::shared_ptr<SimulationState const> const& sim,
                     std::size_t actorIdx) {
    auto const& actorData = sim->_actors[actorIdx];
    auto action = actors[actorIdx]->step(actorData._localAgency);
    if (action != nullptr) {
      // now mutate the agency
      auto newSim = std::make_shared<SimulationState>(*sim);
      action->apply(newSim->_agency);
      activeStates.push_back(newSim);
    }
  }

  // compute all possible next states.
  // each actor can run or load
  void expand(std::shared_ptr<SimulationState const> const& sim) {
    for (std::size_t i = 0; i < actors.size(); i++) {
      createLoadStep(sim, i);
      createRunStep(sim, i);
    }
  }

  void expandAll() {
    std::size_t step_no = 0;
    while (!activeStates.empty()) {
      auto sim = activeStates.front();
      activeStates.pop_front();
      expand(sim);
      std::cout << "executing step " << step_no << ", remaining "
                << activeStates.size() << std::endl;
    }
  }

  void createInitialState(RSA::State state, std::optional<RLA::Log> log) {
    auto newSim = std::make_shared<SimulationState>();
    newSim->_agency = std::make_shared<AgencyState>();
    newSim->_agency->replicatedLog = std::move(log);
    newSim->_agency->replicatedState = std::move(state);
    newSim->_actors.reserve(actors.size());
    for (std::size_t i = 0; i < actors.size(); ++i) {
      newSim->_actors.emplace_back()._localAgency = newSim->_agency;
    }
    activeStates.emplace_back(std::move(newSim));
  }

  LogConfig const defaultConfig = {2, 2, 3, false};
  LogId const logId{12};

  ParticipantFlags const flagsSnapshotComplete{};
  ParticipantFlags const flagsSnapshotIncomplete{.allowedInQuorum = false,
                                                 .allowedAsLeader = false};
};

TEST_F(ReplicatedStateSupervisionSimulationTest, check_state_and_log) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(20)
      .setTargetConfig(defaultConfig);

  actors.emplace_back(std::make_unique<SupervisionStateActor>());
  actors.emplace_back(std::make_unique<DBServerActor>("A"));
  actors.emplace_back(std::make_unique<DBServerActor>("B"));
  actors.emplace_back(std::make_unique<DBServerActor>("C"));

  createInitialState(state.get(), std::nullopt);
  expandAll();
}
