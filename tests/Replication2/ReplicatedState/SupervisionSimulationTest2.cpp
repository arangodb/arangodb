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
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/Predicates.h"

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
    return os;
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

struct SupervisionStateAction {
  void apply(AgencyState& agency) {
    auto actionCtx =
        executeAction(agency.replicatedState, agency.replicatedLog, _action);
    if (actionCtx.hasModificationFor<RLA::LogTarget>()) {
      if (!agency.replicatedLog) {
        agency.replicatedLog.emplace();
      }
      agency.replicatedLog->target = actionCtx.getValue<RLA::LogTarget>();
    }
    if (actionCtx.hasModificationFor<RSA::Plan>()) {
      agency.replicatedState.plan = actionCtx.getValue<RSA::Plan>();
    }
    if (actionCtx.hasModificationFor<RSA::Current::Supervision>()) {
      if (!agency.replicatedState.current) {
        agency.replicatedState.current.emplace();
      }
      agency.replicatedState.current->supervision =
          actionCtx.getValue<RSA::Current::Supervision>();
    }
  }

  explicit SupervisionStateAction(replicated_state::Action action)
      : _action(std::move(action)) {}

  auto toString() const -> std::string {
    return std::string{"Supervision "} +
           std::visit([&](auto const& x) { return typeid(x).name(); }, _action);
  }

  replicated_state::Action _action;
};

struct KillServerAction {
  void apply(AgencyState& agency) {
    agency.health._health.at(id).notIsFailed = false;
  }

  explicit KillServerAction(ParticipantId id) : id(std::move(id)) {}

  auto toString() const -> std::string { return std::string{"kill "} + id; }
  ParticipantId id;
};

struct LoadAgencyData {
  void apply(AgencyState& agency) {}
  explicit LoadAgencyData(ParticipantId id) : id(std::move(id)) {}
  auto toString() const -> std::string { return std::string{"load "} + id; }
  ParticipantId id;
};

struct SupervisionLogAction {
  void apply(AgencyState& agency) {
    auto ctx =
        replicated_log::ActionContext{agency.replicatedLog.value().plan,
                                      agency.replicatedLog.value().current};
    std::visit([&](auto& action) { action.execute(ctx); }, _action);
    if (ctx.hasCurrentModification()) {
      agency.replicatedLog->current = ctx.getCurrent();
    }
    if (ctx.hasPlanModification()) {
      agency.replicatedLog->plan = ctx.getPlan();
    }
  }

  explicit SupervisionLogAction(replicated_log::Action action)
      : _action(std::move(action)) {}
  auto toString() const -> std::string {
    return std::string{"Supervision "} +
           std::visit([&](auto const& x) { return typeid(x).name(); }, _action);
  }

  replicated_log::Action _action;
};

struct DBServerSnapshotCompleteAction {
  explicit DBServerSnapshotCompleteAction(ParticipantId name,
                                          StateGeneration generation)
      : name(std::move(name)), generation(generation) {}

  void apply(AgencyState& agency) {
    if (!agency.replicatedState.current) {
      agency.replicatedState.current.emplace();
    }
    auto& status = agency.replicatedState.current->participants[name];
    status.generation = generation;
    status.snapshot.status = SnapshotStatus::kCompleted;
  };
  auto toString() const -> std::string {
    return std::string{"Snapshot Complete for "} + name + "@" +
           to_string(generation);
  }

  ParticipantId name;
  StateGeneration generation;
};

struct DBServerReportTermAction {
  explicit DBServerReportTermAction(ParticipantId name, LogTerm term)
      : name(std::move(name)), term(term) {}

  void apply(AgencyState& agency) {
    if (!agency.replicatedLog->current) {
      agency.replicatedLog->current.emplace();
    }
    auto& status = agency.replicatedLog->current->localState[name];
    status.term = term;
  };
  auto toString() const -> std::string {
    return std::string{"Report Term for "} + name + ", term" + to_string(term);
  }

  ParticipantId name;
  LogTerm term;
};

struct DBServerCommitConfigAction {
  explicit DBServerCommitConfigAction(ParticipantId name,
                                      std::size_t generation, LogTerm term)
      : name(std::move(name)), generation(generation), term(term) {}

  void apply(AgencyState& agency) {
    if (!agency.replicatedLog->current) {
      agency.replicatedLog->current.emplace();
    }
    if (!agency.replicatedLog->current->leader) {
      agency.replicatedLog->current->leader.emplace();
    }

    auto& leader = *agency.replicatedLog->current->leader;
    leader.leadershipEstablished = true;
    leader.serverId = name;
    leader.term = term;
    leader.committedParticipantsConfig =
        agency.replicatedLog->plan->participantsConfig;
    leader.committedParticipantsConfig->generation = generation;
  };
  auto toString() const -> std::string {
    return std::string{"Commit for"} + name + ", generation " +
           std::to_string(generation) + ", term " + to_string(term);
  }

  ParticipantId name;
  std::size_t generation;
  LogTerm term;
};

using AgencyTransition =
    std::variant<SupervisionStateAction, SupervisionLogAction,
                 DBServerSnapshotCompleteAction, DBServerReportTermAction,
                 DBServerCommitConfigAction, KillServerAction, LoadAgencyData>;

auto operator<<(std::ostream& os, AgencyTransition const& a) -> std::ostream& {
  return os << std::visit([](auto const& action) { return action.toString(); },
                          a);
}

template<typename Derived>
struct ActorBase {
  using ActionVector = std::vector<AgencyTransition>;

  struct InternalState {
    friend auto operator==(InternalState const& lhs,
                           InternalState const& rhs) noexcept -> bool {
      return true;
    }
    friend auto operator<<(std::ostream& os, InternalState const&) noexcept
        -> std::ostream& {
      return os;
    }
    friend auto hash_value(InternalState const& i) noexcept -> std::size_t {
      return 0;
    }
  };

  auto expand(AgencyState const& s, InternalState const& i)
      -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> {
    auto result =
        std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>>{};

    auto actions = reinterpret_cast<Derived const&>(*this).step(s);
    for (auto& action : actions) {
      auto newState = s;
      std::visit([&](auto& action) { action.apply(newState); }, action);
      result.emplace_back(std::move(action), std::move(newState),
                          InternalState{});
    }

    return result;
  }
};

struct SupervisionActor : ActorBase<SupervisionActor> {
  static auto stepReplicatedState(AgencyState const& agency)
      -> std::optional<AgencyTransition> {
    replicated_state::SupervisionContext ctx;
    ctx.enableErrorReporting();
    replicated_state::checkReplicatedState(ctx, agency.replicatedLog,
                                           agency.replicatedState);
    auto action = ctx.getAction();
    if (std::holds_alternative<EmptyAction>(action)) {
      return std::nullopt;
    }
    return SupervisionStateAction{std::move(action)};
  }

  static auto stepReplicatedLog(AgencyState const& agency)
      -> std::optional<AgencyTransition> {
    if (!agency.replicatedLog.has_value()) {
      return std::nullopt;
    }
    auto action = replicated_log::checkReplicatedLog(
        agency.replicatedLog->target, agency.replicatedLog->plan,
        agency.replicatedLog->current, agency.health);
    if (std::holds_alternative<replicated_log::EmptyAction>(action)) {
      return std::nullopt;
    }
    if (std::holds_alternative<replicated_log::LeaderElectionOutOfBoundsAction>(
            action)) {
      return std::nullopt;
    }
    return SupervisionLogAction{std::move(action)};
  }

  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition> {
    std::vector<AgencyTransition> v;
    if (auto action = stepReplicatedLog(agency); action) {
      v.emplace_back(std::move(*action));
    }
    if (auto action = stepReplicatedState(agency); action) {
      v.emplace_back(std::move(*action));
    }
    return v;
  }
};

struct DBServerActor : ActorBase<DBServerActor> {
  explicit DBServerActor(ParticipantId name) : name(std::move(name)) {}

  [[nodiscard]] auto stepReplicatedState(AgencyState const& agency) const
      -> std::optional<AgencyTransition> {
    if (auto& plan = agency.replicatedState.plan; plan.has_value()) {
      if (auto iter = plan->participants.find(name);
          iter != plan->participants.end()) {
        auto wantedGeneration = iter->second.generation;

        if (auto current = agency.replicatedState.current;
            current.has_value()) {
          if (auto iter2 = current->participants.find(name);
              iter2 != current->participants.end()) {
            auto& status = iter2->second;
            if (status.generation == wantedGeneration &&
                status.snapshot.status == SnapshotStatus::kCompleted) {
              return std::nullopt;
            }
          }
        }

        return DBServerSnapshotCompleteAction{name, wantedGeneration};
      }
    }

    return std::nullopt;
  }

  auto stepReplicatedLogReportTerm(AgencyState const& agency) const
      -> std::optional<AgencyTransition> {
    if (!agency.replicatedLog || !agency.replicatedLog->plan) {
      return std::nullopt;
    }
    auto const& plan = *agency.replicatedLog->plan;
    auto reportedTerm = std::invoke([&] {
      if (agency.replicatedLog->current) {
        auto const& current = *agency.replicatedLog->current;
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
        return DBServerReportTermAction{name, term.term};
      }
    }
    return std::nullopt;
  }

  auto stepReplicatedLogLeaderCommit(AgencyState const& agency) const
      -> std::optional<AgencyTransition> {
    if (!agency.replicatedLog || !agency.replicatedLog->plan) {
      return std::nullopt;
    }
    auto committedGeneration = std::invoke([&]() -> std::size_t {
      if (agency.replicatedLog->current) {
        auto const& current = *agency.replicatedLog->current;
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
    auto const& plan = *agency.replicatedLog->plan;
    if (plan.currentTerm) {
      auto const& term = *plan.currentTerm;
      if (term.leader && term.leader->serverId == name) {
        if (plan.participantsConfig.generation != committedGeneration) {
          return DBServerCommitConfigAction{
              name, plan.participantsConfig.generation, term.term};
        }
      }
    }
    return std::nullopt;
  }

  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition> {
    std::vector<AgencyTransition> v;
    if (auto action = stepReplicatedState(agency); action) {
      v.emplace_back(std::move(*action));
    }
    if (auto action = stepReplicatedLogReportTerm(agency); action) {
      v.emplace_back(std::move(*action));
    }
    if (auto action = stepReplicatedLogLeaderCommit(agency); action) {
      v.emplace_back(std::move(*action));
    }
    return v;
  }

  ParticipantId name;
};

struct KillLeaderActor : ActorBase<KillLeaderActor> {
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition> {
    std::vector<AgencyTransition> v;

    if (agency.replicatedLog && agency.replicatedLog->plan) {
      if (agency.replicatedLog->plan->currentTerm) {
        auto const& term = *agency.replicatedLog->plan->currentTerm;
        if (term.term == LogTerm{1} && term.leader) {
          auto const& health = agency.health;
          auto const& leader = *term.leader;
          auto isHealth =
              health.validRebootId(leader.serverId, leader.rebootId) &&
              health.notIsFailed(leader.serverId);
          if (isHealth) {
            v.emplace_back(KillServerAction{leader.serverId});
          }
        }
      }
    }

    return v;
  }
};
}  // namespace

struct ReplicatedStateSupervisionSimulationTest2 : ::testing::Test {
  LogConfig const defaultConfig = {2, 2, 3, false};
  LogId const logId{12};

  ParticipantFlags const flagsSnapshotComplete{};
  ParticipantFlags const flagsSnapshotIncomplete{.allowedInQuorum = false,
                                                 .allowedAsLeader = false};
};

namespace {
auto isLeaderHealth() {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan &&
        state.replicatedLog->plan->currentTerm) {
      auto const& term = *state.replicatedLog->plan->currentTerm;
      if (term.leader) {
        auto const& leader = *term.leader;
        auto const& health = global.state.health;
        return health.validRebootId(leader.serverId, leader.rebootId) &&
               health.notIsFailed(leader.serverId);
      }
    }
    return false;
  });
}
}  // namespace

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

  auto test = MC_EVENTUALLY_ALWAYS(isLeaderHealth());
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;
  auto result = Engine::run(driver, test, initState);
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

  auto test = MC_EVENTUALLY_ALWAYS(isLeaderHealth());
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, test, initState);
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
      .setTargetParticipant("C", flagsSnapshotComplete)
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

  auto test = MC_EVENTUALLY_ALWAYS(isLeaderHealth());
  using Engine = model_checker::ActorEngine<AgencyState, AgencyTransition>;

  auto result = Engine::run(driver, test, initState);
  EXPECT_FALSE(result.failed) << *result.failed;
  std::cout << result.stats << std::endl;
}
