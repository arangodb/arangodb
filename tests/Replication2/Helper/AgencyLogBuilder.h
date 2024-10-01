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

#pragma once
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

namespace RLA = arangodb::replication2::agency;

namespace arangodb::test {

struct AgencyLogBuilder {
  AgencyLogBuilder() = default;

  auto setId(replication2::LogId id) -> AgencyLogBuilder& {
    _log.target.id = id;
    if (_log.plan) {
      _log.plan->id = id;
    }
    return *this;
  }

  auto setTargetParticipant(replication2::ParticipantId const& id,
                            replication2::ParticipantFlags flags)
      -> AgencyLogBuilder& {
    _log.target.participants[id] = flags;
    return *this;
  }

  auto setTargetConfig(RLA::LogTargetConfig config) -> AgencyLogBuilder& {
    _log.target.config = config;
    return *this;
  }

  auto setPlanConfig(RLA::LogPlanConfig config) -> AgencyLogBuilder& {
    makePlan().participantsConfig.config = config;
    return *this;
  }

  auto setTargetLeader(std::optional<replication2::ParticipantId> leader)
      -> AgencyLogBuilder& {
    _log.target.leader = std::move(leader);
    return *this;
  }

  auto setTargetVersion(std::optional<std::uint64_t> version)
      -> AgencyLogBuilder& {
    _log.target.version = version;
    return *this;
  }

  auto setCurrentVersion(std::optional<std::uint64_t> version)
      -> AgencyLogBuilder& {
    makeCurrent().supervision->targetVersion = version;
    return *this;
  }

  auto makeTerm() -> RLA::LogPlanTermSpecification& {
    auto& plan = makePlan();
    if (!plan.currentTerm.has_value()) {
      plan.currentTerm.emplace();
      plan.currentTerm->term = replication2::LogTerm{1};
      plan.participantsConfig.config = RLA::LogPlanConfig(
          _log.target.config.writeConcern, _log.target.config.waitForSync);
    }
    return plan.currentTerm.value();
  }

  auto setPlanLeader(replication2::ParticipantId const& id,
                     RebootId rid = RebootId{0}) -> AgencyLogBuilder& {
    makeTerm().leader.emplace(id, rid);
    return *this;
  }

  auto setPlanParticipant(replication2::ParticipantId const& id,
                          replication2::ParticipantFlags flags)
      -> AgencyLogBuilder& {
    makePlan().participantsConfig.participants[id] = flags;
    return *this;
  }

  auto makePlan() -> RLA::LogPlanSpecification& {
    if (!_log.plan.has_value()) {
      _log.plan.emplace();
      _log.plan->id = _log.target.id;
      _log.plan->participantsConfig.generation = 1;
    }
    return _log.plan.value();
  }

  auto establishLeadership() -> AgencyLogBuilder& {
    auto& leader = makeCurrent().leader.emplace();
    leader.term = makeTerm().term;
    leader.leadershipEstablished = true;
    leader.serverId = makeTerm().leader.value().serverId;
    leader.committedParticipantsConfig = makePlan().participantsConfig;
    return *this;
  }

  auto setEmptyTerm() -> AgencyLogBuilder& {
    auto& term = makeTerm();

    term.leader.reset();
    term.term.value++;
    return *this;
  }

  auto acknowledgeTerm(replication2::ParticipantId const& id)
      -> AgencyLogBuilder& {
    auto& current = makeCurrent();
    current.localState[id].term = makeTerm().term;
    return *this;
  }

  auto setSnapshotTrue(replication2::ParticipantId const& id)
      -> AgencyLogBuilder& {
    auto& current = makeCurrent();
    current.localState[id].snapshotAvailable = true;
    return *this;
  }

  auto allSnapshotsTrue() -> AgencyLogBuilder& {
    auto& current = makeCurrent();
    for (auto& [id, v] : current.localState) {
      v.snapshotAvailable = true;
    }
    return *this;
  }

  auto allStatesReady() -> AgencyLogBuilder& {
    auto& current = makeCurrent();
    for (auto& [id, v] : current.localState) {
      v.state =
          replication2::replicated_log::LocalStateMachineStatus::kOperational;
    }
    return *this;
  }

  auto makeCurrent() -> RLA::LogCurrent& {
    if (!_log.current.has_value()) {
      _log.current.emplace();

      if (!_log.current->supervision.has_value()) {
        _log.current->supervision.emplace();
      }
      // makeCurrent should really only be called if a plan already exists.
      if (_log.plan.has_value()) {
        _log.current->supervision->assumedWriteConcern =
            _log.plan->participantsConfig.config.effectiveWriteConcern;
        _log.current->supervision->assumedWaitForSync =
            _log.plan->participantsConfig.config.waitForSync;
      }
    }
    return _log.current.value();
  }

  auto setPlanConfigGeneration(std::size_t generation) {
    auto& plan = makePlan();
    plan.participantsConfig.generation = generation;
  }

  auto commitCurrentParticipantsConfig() {
    auto& plan = makePlan();
    auto& current = makeCurrent();
    establishLeadership();
    current.leader->committedParticipantsConfig = plan.participantsConfig;
  }

  auto get() const noexcept -> RLA::Log const& { return _log; }
  RLA::Log _log;
};
}  // namespace arangodb::test
