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
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

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
  boost::hash_combine(seed, s.participants);
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
  boost::hash_combine(seed, s.participants);
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
  boost::hash_combine(seed, s.participants);
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
namespace std {
template<typename T, typename K>
auto hash_value(std::unordered_map<K, T> const& m) {
  std::size_t seed = 0;
  for (auto const& [k, v] : m) {
    std::size_t subseed = 0;
    boost::hash_combine(subseed, v);
    boost::hash_combine(subseed, k);
    seed ^= subseed;
  }
  return seed;
};
}  // namespace std
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
  boost::hash_combine(seed, s.participants);
  return seed;
}
}  // namespace arangodb::replication2
namespace arangodb::replication2::agency {

std::size_t hash_value(LogTarget const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.version);
  boost::hash_combine(seed, s.leader);
  boost::hash_combine(seed, s.participants);
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
  boost::hash_combine(seed, s.localState);
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
namespace arangodb::replication2::replicated_log {
std::size_t hash_value(ParticipantHealth const& h) {
  std::size_t seed = 0;
  boost::hash_combine(seed, h.rebootId.value());
  boost::hash_combine(seed, h.notIsFailed);
  return seed;
}
std::size_t hash_value(ParticipantsHealth const& h) {
  return hash_value(h._health);
}
}  // namespace arangodb::replication2::replicated_log
namespace {

struct AgencyState {
  RSA::State replicatedState;
  std::optional<RLA::Log> replicatedLog;
  replicated_log::ParticipantsHealth health;

  friend std::size_t hash_value(AgencyState const& s) {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.replicatedState);
    boost::hash_combine(seed, s.replicatedLog);
    boost::hash_combine(seed, s.health);
    return seed;
  }
  friend auto operator==(AgencyState const& s, AgencyState const& s2) noexcept
      -> bool = default;

  friend auto operator<<(std::ostream& os, AgencyState const& state)
      -> std::ostream& {
    auto const print = [&](auto const& x) {
      VPackBuilder builder;
      x.toVelocyPack(builder);
      os << builder.toJson() << std::endl;
    };

    print(state.replicatedState.target);
    if (state.replicatedState.plan) {
      print(*state.replicatedState.plan);
    }
    if (state.replicatedState.current) {
      print(*state.replicatedState.current);
    }
    if (state.replicatedLog) {
      print(state.replicatedLog->target);
      if (state.replicatedLog->plan) {
        print(*state.replicatedLog->plan);
      }
      if (state.replicatedLog->current) {
        print(*state.replicatedLog->current);
      }
    }
    for (auto const& [name, ph] : state.health._health) {
      std::cout << name << " reboot id = " << ph.rebootId.value()
                << " failed = " << !ph.notIsFailed;
    }
    return os;
  }
};

struct ActorState {
  bool wantLoad = false;
  std::shared_ptr<AgencyState> _localAgency;

  friend std::size_t hash_value(ActorState const& sim) {
    std::size_t seed = 0;
    boost::hash_combine(seed, *sim._localAgency);
    boost::hash_combine(seed, sim.wantLoad);
    return seed;
  }
  friend auto operator==(ActorState const& s, ActorState const& s2) noexcept
      -> bool = default;
};

struct SimulationAction {
  virtual ~SimulationAction() = default;
  virtual void apply(std::shared_ptr<AgencyState>) = 0;
  auto toString() const -> std::string {
    std::stringstream ss;
    this->operator<<(ss);
    return ss.str();
  }
  virtual auto operator<<(std::ostream&) const -> std::ostream& = 0;
};

struct LoadAction final : SimulationAction {
  explicit LoadAction(std::size_t index) : actorIndex(index) {}
  void apply(std::shared_ptr<AgencyState>) override {}
  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Load " << actorIndex;
  }
  std::size_t actorIndex;
};

struct SimulationState {
  std::size_t depth = 0;
  std::size_t _uniqueId = 0;
  std::shared_ptr<AgencyState> _agency;
  std::vector<ActorState> _actors;
  mutable std::vector<std::pair<std::shared_ptr<SimulationState const>,
                                std::unique_ptr<SimulationAction>>>
      _previous;

  friend std::size_t hash_value(SimulationState const& sim) {
    std::size_t seed = 0;
    boost::hash_combine(seed, *sim._agency);
    boost::hash_combine(seed, sim._actors);
    return seed;
  }

  friend auto operator==(SimulationState const& s, SimulationState const& s2)
      -> bool {
    return std::tie(*s._agency, s._actors) == std::tie(*s2._agency, s2._actors);
  }
};

struct SupervisionStateAction final : SimulationAction {
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

  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Supervision "
              << std::visit([&](auto const& x) { return typeid(x).name(); },
                            _action);
  }

  replicated_state::Action _action;
};

struct SupervisionLogAction final : SimulationAction {
  void apply(std::shared_ptr<AgencyState> agency) override {
    auto ctx =
        replicated_log::ActionContext{agency->replicatedLog.value().plan,
                                      agency->replicatedLog.value().current};
    std::visit([&](auto& action) { action.execute(ctx); }, _action);
    if (ctx.hasCurrentModification()) {
      agency->replicatedLog->current = ctx.getCurrent();
    }
    if (ctx.hasPlanModification()) {
      agency->replicatedLog->plan = ctx.getPlan();
    }
  }

  explicit SupervisionLogAction(replicated_log::Action action)
      : _action(std::move(action)) {}
  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Supervision "
              << std::visit([&](auto const& x) { return typeid(x).name(); },
                            _action);
  }

  replicated_log::Action _action;
};

struct Actor {
  virtual ~Actor() = default;
  virtual auto step(std::shared_ptr<AgencyState const> const&) const
      -> std::vector<std::unique_ptr<SimulationAction>> = 0;
};

struct SupervisionActor final : Actor {
  static auto stepReplicatedState(
      std::shared_ptr<AgencyState const> const& agency)
      -> std::unique_ptr<SimulationAction> {
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

  static auto stepReplicatedLog(
      std::shared_ptr<AgencyState const> const& agency)
      -> std::unique_ptr<SimulationAction> {
    if (!agency->replicatedLog.has_value()) {
      return nullptr;
    }
    auto action = replicated_log::checkReplicatedLog(
        agency->replicatedLog->target, agency->replicatedLog->plan,
        agency->replicatedLog->current, agency->health);
    if (std::holds_alternative<replicated_log::EmptyAction>(action)) {
      return nullptr;
    }
    if (std::holds_alternative<replicated_log::LeaderElectionOutOfBoundsAction>(
            action)) {
      return nullptr;
    }
    return std::make_unique<SupervisionLogAction>(std::move(action));
  }

  auto step(std::shared_ptr<AgencyState const> const& agency) const
      -> std::vector<std::unique_ptr<SimulationAction>> override {
    std::vector<std::unique_ptr<SimulationAction>> v;
    if (auto action = stepReplicatedLog(agency); action != nullptr) {
      v.emplace_back(std::move(action));
    }
    if (auto action = stepReplicatedState(agency); action != nullptr) {
      v.emplace_back(std::move(action));
    }
    return v;
  }
};

struct DBServerSnapshotCompleteAction final : SimulationAction {
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
  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Snapshot Complete for " << name << "@" << generation;
  }

  ParticipantId name;
  StateGeneration generation;
};

struct DBServerReportTermAction final : SimulationAction {
  explicit DBServerReportTermAction(ParticipantId name, LogTerm term)
      : name(std::move(name)), term(term) {}

  void apply(std::shared_ptr<AgencyState> agency) override {
    if (!agency->replicatedLog->current) {
      agency->replicatedLog->current.emplace();
    }
    auto& status = agency->replicatedLog->current->localState[name];
    status.term = term;
  };
  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Report Term for " << name << ", term" << term;
  }

  ParticipantId name;
  LogTerm term;
};

struct DBServerCommitConfigAction final : SimulationAction {
  explicit DBServerCommitConfigAction(ParticipantId name,
                                      std::size_t generation, LogTerm term)
      : name(std::move(name)), generation(generation), term(term) {}

  void apply(std::shared_ptr<AgencyState> agency) override {
    if (!agency->replicatedLog->current) {
      agency->replicatedLog->current.emplace();
    }
    if (!agency->replicatedLog->current->leader) {
      agency->replicatedLog->current->leader.emplace();
    }

    auto& leader = *agency->replicatedLog->current->leader;
    leader.leadershipEstablished = true;
    leader.serverId = name;
    leader.term = term;
    leader.committedParticipantsConfig =
        agency->replicatedLog->plan->participantsConfig;
    leader.committedParticipantsConfig->generation = generation;
  };
  auto operator<<(std::ostream& os) const -> std::ostream& override {
    return os << "Commit for" << name << ", generation " << generation
              << ", term " << term;
  }

  ParticipantId name;
  std::size_t generation;
  LogTerm term;
};

struct DBServerActor final : Actor {
  explicit DBServerActor(ParticipantId name) : name(std::move(name)) {}

  [[nodiscard]] auto stepReplicatedState(
      std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<SimulationAction> {
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

  auto stepReplicatedLogReportTerm(
      std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<SimulationAction> {
    if (!agency->replicatedLog || !agency->replicatedLog->plan) {
      return nullptr;
    }
    auto const& plan = *agency->replicatedLog->plan;
    auto reportedTerm = std::invoke([&] {
      if (agency->replicatedLog->current) {
        auto const& current = *agency->replicatedLog->current;
        auto iter = current.localState.find(name);
        if (iter != current.localState.end()) {
          return iter->second.term;
        }
      }
      return LogTerm{0};
    });

    // check if we have to report a new term in current
    if (plan.currentTerm) {
      auto const& term = *plan.currentTerm;
      if (term.term != reportedTerm) {
        return std::make_unique<DBServerReportTermAction>(name, term.term);
      }
    }
    return nullptr;
  }

  auto stepReplicatedLogLeaderCommit(
      std::shared_ptr<AgencyState const> const& agency) const
      -> std::unique_ptr<SimulationAction> {
    if (!agency->replicatedLog || !agency->replicatedLog->plan) {
      return nullptr;
    }
    auto committedGeneration = std::invoke([&]() -> std::size_t {
      if (agency->replicatedLog->current) {
        auto const& current = *agency->replicatedLog->current;
        if (current.leader) {
          auto const& leader = *current.leader;
          if (leader.serverId == name) {
            if (leader.leadershipEstablished &&
                leader.committedParticipantsConfig) {
              return leader.committedParticipantsConfig->generation;
            }
          }
        }
      }
      return 0;
    });
    auto const& plan = *agency->replicatedLog->plan;
    if (plan.currentTerm) {
      auto const& term = *plan.currentTerm;
      if (term.leader->serverId == name) {
        if (plan.participantsConfig.generation != committedGeneration) {
          return std::make_unique<DBServerCommitConfigAction>(
              name, plan.participantsConfig.generation, term.term);
        }
      }
    }
    return nullptr;
  }

  auto step(std::shared_ptr<AgencyState const> const& agency) const
      -> std::vector<std::unique_ptr<SimulationAction>> override {
    std::vector<std::unique_ptr<SimulationAction>> v;
    if (auto action = stepReplicatedState(agency); action != nullptr) {
      v.emplace_back(std::move(action));
    }
    if (auto action = stepReplicatedLogReportTerm(agency); action != nullptr) {
      v.emplace_back(std::move(action));
    }
    if (auto action = stepReplicatedLogLeaderCommit(agency);
        action != nullptr) {
      v.emplace_back(std::move(action));
    }
    return v;
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
  std::unordered_set<std::shared_ptr<SimulationState>, SimStateHash,
                     SimStateCmp>
      fingerprints;

  std::size_t numCreateStates = 0;
  std::size_t discoveredStates = 0;
  std::size_t eliminatedStates = 0;

  void addNewState(std::shared_ptr<SimulationState> const& newState,
                   std::shared_ptr<SimulationState const> const& prev,
                   std::unique_ptr<SimulationAction> action) {
    newState->_uniqueId = numCreateStates += 1;
    auto [ptr, inserted] = fingerprints.emplace(newState);
    if (inserted) {
      activeStates.emplace_back(newState);
      discoveredStates += 1;
      newState->_previous.emplace_back(std::pair{prev, std::move(action)});
    } else {
      ptr->operator*()._previous.emplace_back(
          std::pair{prev, std::move(action)});
      eliminatedStates += 1;
    }
  }

  void createLoadStep(std::shared_ptr<SimulationState const> const& sim,
                      std::size_t actorIdx) {
    // first check if there is something new
    auto const& actor = sim->_actors[actorIdx];
    auto newSim = std::make_shared<SimulationState>();
    newSim->_actors = sim->_actors;
    newSim->depth = sim->depth + 1;
    newSim->_agency = sim->_agency;
    newSim->_actors[actorIdx]._localAgency = sim->_agency;
    newSim->_actors[actorIdx].wantLoad = false;
    addNewState(newSim, sim, std::make_unique<LoadAction>(actorIdx));
  }

  void createRunStep(std::shared_ptr<SimulationState const> const& sim,
                     std::size_t actorIdx) {
    auto& actorData = sim->_actors[actorIdx];
    if (actorData.wantLoad) {
      createLoadStep(sim, actorIdx);
    } else {
      auto actions = actors[actorIdx]->step(actorData._localAgency);
      for (auto& action : actions) {
        // now mutate the agency
        auto newSim = std::make_shared<SimulationState>();
        newSim->_agency = std::make_shared<AgencyState>(*sim->_agency);
        action->apply(newSim->_agency);
        newSim->_actors = sim->_actors;
        newSim->_actors[actorIdx].wantLoad = true;
        newSim->depth = sim->depth + 1;
        addNewState(newSim, sim, std::move(action));
      }
    }
  }

  // compute all possible next states.
  // each actor can run or load
  void expand(std::shared_ptr<SimulationState const> const& sim) {
    for (std::size_t i = 0; i < actors.size(); i++) {
      // createLoadStep(sim, i);
      createRunStep(sim, i);
    }
  }

  void expandAll() {
    std::size_t step_no = 0;
    auto last = std::chrono::steady_clock::now();
    while (!activeStates.empty()) {
      auto sim = activeStates.front();
      activeStates.pop_front();
      auto oldSize = numCreateStates;
      expand(sim);
      if (oldSize == numCreateStates) {
        std::cout << sim->_uniqueId << std::endl;
        std::cout << *sim->_agency << std::endl;
        printPath(sim);
      }
      if (std::chrono::steady_clock::now() - last > std::chrono::seconds{5}) {
        std::cout << "total states = " << discoveredStates
                  << "; eliminated = " << eliminatedStates
                  << "; active = " << activeStates.size() << std::endl;
        last = std::chrono::steady_clock::now();
        printPath(sim);
      }
    }
    std::cout << "end of expandAll" << std::endl;
  }

  void printPath(std::shared_ptr<SimulationState const> const& sim) {
    if (!sim->_previous.empty()) {
      printPath(sim->_previous[0].first);
      std::cout << sim->_previous[0].second->toString() << std::endl;
    }
  }

  void printAllStates() {
    std::size_t idx = 0;
    std::cout << "digraph foobar {" << std::endl;
    for (auto const& s : fingerprints) {
      std::cout << "v" << s->_uniqueId;
      if (s->_uniqueId == 0) {
        std::cout << "[label=\"initial\"]";
      }
      std::cout << ";" << std::endl;
    }

    for (auto const& s : fingerprints) {
      for (auto const& [p, action] : s->_previous) {
        std::cout << "v" << p->_uniqueId << " -> v" << s->_uniqueId
                  << "[label=\"" << action->toString() << "\"];" << std::endl;
      }
    }

    std::cout << "}" << std::endl;
  }

  void setServerHealth(ParticipantId const& id, RebootId rebootId,
                       bool isFailed) {
    activeStates.front()->_agency->health._health.emplace(
        id, replicated_log::ParticipantHealth{rebootId, !isFailed});
  }

  void createInitialState(RSA::State state, std::optional<RLA::Log> log) {
    auto newSim = std::make_shared<SimulationState>();
    newSim->_agency = std::make_shared<AgencyState>();
    newSim->_agency->replicatedLog = std::move(log);
    newSim->_agency->replicatedState = std::move(state);
    newSim->_actors.reserve(actors.size());
    for (std::size_t i = 0; i < actors.size(); ++i) {
      auto& actor = newSim->_actors.emplace_back();
      actor._localAgency = newSim->_agency;
      actor.wantLoad = true;
    }
    activeStates.emplace_back(newSim);
    fingerprints.emplace(std::move(newSim));
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
  actors.emplace_back(std::make_unique<DBServerActor>("C"));

  createInitialState(state.get(), std::nullopt);
  setServerHealth("A", RebootId{1}, false);
  setServerHealth("B", RebootId{1}, false);
  setServerHealth("C", RebootId{1}, false);
  expandAll();

  std::cout << "total states = " << discoveredStates
            << "; eliminated = " << eliminatedStates << std::endl;
  // printAllStates();
}
