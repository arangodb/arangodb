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

#include <boost/container_hash/hash.hpp>

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

namespace arangodb::replication2::replicated_state::agency {
std::size_t hash_value(Target const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.version);
  boost::hash_combine(seed, s.leader);
  boost::hash_combine(
      seed, boost::hash_range(s.participants.begin(), s.participants.end()));
  return seed;
}
std::size_t hash_value(RSA::Target::Participant const& s) { return 1; }
std::size_t hash_value(RSA::Plan::Participant const& p) {
  return boost::hash_value(p.generation.value);
}
std::size_t hash_value(Plan const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.generation.value);
  boost::hash_combine(
      seed, boost::hash_range(s.participants.begin(), s.participants.end()));
  return seed;
}
std::size_t hash_value(Current::ParticipantStatus const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.generation.value);
  boost::hash_combine(seed, s.snapshot.status);
  return seed;
}
std::size_t hash_value(Current::Supervision const& s) {
  return boost::hash_value(s.version);
}
std::size_t hash_value(Current const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.supervision);
  boost::hash_combine(
      seed, boost::hash_range(s.participants.begin(), s.participants.end()));
  return seed;
}
std::size_t hash_value(State const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.target);
  boost::hash_combine(seed, s.plan);
  boost::hash_combine(seed, s.current);
  return seed;
}
}  // namespace arangodb::replication2::replicated_state::agency
namespace arangodb::replication2 {
std::size_t hash_value(ParticipantFlags const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.allowedAsLeader);
  boost::hash_combine(seed, s.allowedInQuorum);
  boost::hash_combine(seed, s.forced);
  return seed;
}
std::size_t hash_value(ParticipantsConfig const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.generation);
  boost::hash_combine(
      seed, boost::hash_range(s.participants.begin(), s.participants.end()));
  return seed;
}
}  // namespace arangodb::replication2

namespace arangodb::replication2::agency {

std::size_t hash_value(LogTarget const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.version);
  boost::hash_combine(seed, s.leader);
  boost::hash_combine(
      seed, boost::hash_range(s.participants.begin(), s.participants.end()));
  return seed;
}
std::size_t hash_value(LogCurrent::Leader const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.serverId);
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.leadershipEstablished);
  return seed;
}
std::size_t hash_value(LogCurrentLocalState const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.spearhead.index.value);
  boost::hash_combine(seed, s.spearhead.term.value);
  return seed;
}

std::size_t hash_value(LogCurrent const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.targetVersion);
  boost::hash_combine(
      seed, boost::hash_range(s.localState.begin(), s.localState.end()));
  boost::hash_combine(seed, s.leader);
  return seed;
}
std::size_t hash_value(LogPlanTermSpecification::Leader const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.serverId);
  boost::hash_combine(seed, s.rebootId.value());
  return seed;
}

std::size_t hash_value(LogPlanTermSpecification const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.leader);
  return seed;
}
std::size_t hash_value(LogPlanSpecification const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.currentTerm);
  boost::hash_combine(seed, s.participantsConfig);

  return seed;
}

std::size_t hash_value(Log const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.target);
  boost::hash_combine(seed, s.plan);
  boost::hash_combine(seed, s.current);
  return seed;
}
}  // namespace arangodb::replication2::agency

namespace {

struct AgencyState {
  RSA::State replicatedState;
  std::optional<RLA::Log> replicatedLog;

  friend std::size_t hash_value(AgencyState const& s) {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.replicatedState);
    boost::hash_combine(seed, s.replicatedLog);
    return seed;
  }
  friend auto operator==(AgencyState const& s, AgencyState const& s2) noexcept
      -> bool = default;
};

struct ActorState {
  std::shared_ptr<AgencyState> _localAgency;

  friend std::size_t hash_value(ActorState const& sim) {
    return hash_value(*sim._localAgency);
  }
  friend auto operator==(ActorState const& s, ActorState const& s2) noexcept
      -> bool = default;
};

struct Action {
  virtual ~Action() = default;
  virtual void apply(std::shared_ptr<AgencyState>) = 0;
  virtual void print() = 0;
};

struct LoadAction final : Action {
  explicit LoadAction(std::size_t index) : actorIndex(index) {}
  void apply(std::shared_ptr<AgencyState>) override {}
  void print() override { std::cout << "Load " << actorIndex << std::endl; }
  std::size_t actorIndex;
};

struct SimulationState {
  std::size_t depth = 0;
  std::unique_ptr<Action> _producedBy;
  std::shared_ptr<AgencyState> _agency;
  std::vector<ActorState> _actors;
  std::shared_ptr<SimulationState const> _previous;

  friend std::size_t hash_value(SimulationState const& sim) {
    std::size_t seed = 0;
    boost::hash_combine(seed, *sim._agency);
    boost::hash_combine(
        seed, boost::hash_range(sim._actors.begin(), sim._actors.end()));

    return seed;
  }

  friend auto operator==(SimulationState const& s, SimulationState const& s2)
      -> bool {
    return std::tie(*s._agency, s._actors) == std::tie(*s2._agency, s2._actors);
  }
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

  void print() override {
    std::cout << "Supervision "
              << std::visit([&](auto const& x) { return typeid(x).name(); },
                            _action)
              << std::endl;
  }

  replicated_state::Action _action;
};

struct Actor {
  virtual ~Actor() = default;
  virtual auto step(std::shared_ptr<AgencyState const> const&) const
      -> std::unique_ptr<Action> = 0;
};

struct SupervisionActor final : Actor {
  auto stepReplicatedState(std::shared_ptr<AgencyState const> const& agency)
      const -> std::unique_ptr<Action> {
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

  auto step(std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<Action> override {
    return stepReplicatedState(agency);
  }
};

struct DBServerSnapshotCompleteAction final : Action {
  explicit DBServerSnapshotCompleteAction(ParticipantId name,
                                          StateGeneration generation)
      : name(std::move(name)), generation(generation) {}

  void apply(std::shared_ptr<AgencyState> agency) override {
    if (!agency->replicatedState.current) {
      agency->replicatedState.current.emplace();
    }
    auto& status = agency->replicatedState.current->participants[name];
    status.generation = generation;
    status.snapshot.status = SnapshotStatus::kCompleted;
  };

  void print() override {
    std::cout << "Snapshot Complete for " << name << "@" << generation
              << std::endl;
  }
  ParticipantId name;
  StateGeneration generation;
};

struct DBServerActor final : Actor {
  explicit DBServerActor(ParticipantId name) : name(std::move(name)) {}

  auto stepReplicatedState(std::shared_ptr<AgencyState const> const& agency)
      const -> std::unique_ptr<Action> {
    if (auto& plan = agency->replicatedState.plan; plan.has_value()) {
      if (auto iter = plan->participants.find(name);
          iter != plan->participants.end()) {
        auto wantedGeneration = iter->second.generation;

        if (auto current = agency->replicatedState.current;
            current.has_value()) {
          if (auto iter2 = current->participants.find(name);
              iter2 != current->participants.end()) {
            auto& status = iter2->second;
            if (status.generation == wantedGeneration &&
                status.snapshot.status == SnapshotStatus::kCompleted) {
              return nullptr;
            }
          }
        }

        return std::make_unique<DBServerSnapshotCompleteAction>(
            name, wantedGeneration);
      }
    }

    return nullptr;
  }

  auto step(std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<Action> override {
    return stepReplicatedState(agency);
  }

  ParticipantId name;
};

struct SimStateHash {
  auto operator()(std::shared_ptr<SimulationState const> const& p) const {
    return hash_value(*p);
  }
};
struct SimStateCmp {
  auto operator()(std::shared_ptr<SimulationState const> const& p,
                  std::shared_ptr<SimulationState const> const& p2) const {
    return *p == *p2;
  }
};

}  // namespace

struct ReplicatedStateSupervisionSimulationTest : ::testing::Test {
  std::deque<std::shared_ptr<SimulationState const>> activeStates;
  std::vector<std::unique_ptr<Actor>> actors;
  std::unordered_set<std::shared_ptr<SimulationState const>, SimStateHash,
                     SimStateCmp>
      fingerprints;

  std::size_t discoveredStates = 0;
  std::size_t eliminatedStates = 0;

  void addNewState(std::shared_ptr<SimulationState const> const& sim) {
    auto [ptr, inserted] = fingerprints.emplace(sim);
    if (inserted) {
      activeStates.emplace_back(sim);
      discoveredStates += 1;
    } else {
      eliminatedStates += 1;
    }
  }

  void createLoadStep(std::shared_ptr<SimulationState const> const& sim,
                      std::size_t actorIdx) {
    // first check if there is something new
    auto const& actor = sim->_actors[actorIdx];
    if (actor._localAgency != sim->_agency) {
      auto newSim = std::make_shared<SimulationState>();
      newSim->_actors = sim->_actors;
      newSim->depth = sim->depth + 1;
      newSim->_agency = sim->_agency;
      newSim->_previous = sim;
      newSim->_actors[actorIdx]._localAgency = sim->_agency;
      newSim->_producedBy = std::make_unique<LoadAction>(actorIdx);
      addNewState(newSim);
    }
  }

  void createRunStep(std::shared_ptr<SimulationState const> const& sim,
                     std::size_t actorIdx) {
    auto const& actorData = sim->_actors[actorIdx];
    auto action = actors[actorIdx]->step(actorData._localAgency);
    if (action != nullptr) {
      // now mutate the agency
      auto newSim = std::make_shared<SimulationState>();
      newSim->_agency = std::make_shared<AgencyState>(*sim->_agency);
      action->apply(newSim->_agency);
      newSim->_actors = sim->_actors;
      newSim->_actors[actorIdx]._localAgency = newSim->_agency;
      newSim->_producedBy = std::move(action);
      newSim->depth = sim->depth + 1;
      newSim->_previous = sim;
      addNewState(newSim);
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
    auto last = std::chrono::steady_clock::now();
    while (!activeStates.empty()) {
      auto sim = activeStates.front();
      activeStates.pop_front();
      auto oldSize = activeStates.size();
      expand(sim);
      if (std::chrono::steady_clock::now() - last > std::chrono::seconds{5}) {
        std::cout << "total states = " << discoveredStates
                  << "; eliminated = " << eliminatedStates
                  << "; active = " << activeStates.size() << std::endl;
        last = std::chrono::steady_clock::now();
      }
    }
  }

  void printPath(std::shared_ptr<SimulationState const> const& step) {
    if (step->_previous) {
      printPath(step->_previous);
      step->_producedBy->print();
      if (step->_agency->replicatedState.plan) {
        VPackBuilder builder;
        step->_agency->replicatedState.plan->toVelocyPack(builder);
        std::cout << builder.toJson() << std::endl;
      }
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

  actors.emplace_back(std::make_unique<SupervisionActor>());
  actors.emplace_back(std::make_unique<DBServerActor>("A"));
  actors.emplace_back(std::make_unique<DBServerActor>("B"));
  // actors.emplace_back(std::make_unique<DBServerActor>("C"));

  createInitialState(state.get(), std::nullopt);
  expandAll();

  std::cout << "total states = " << discoveredStates
            << "; eliminated = " << eliminatedStates << std::endl;
}
