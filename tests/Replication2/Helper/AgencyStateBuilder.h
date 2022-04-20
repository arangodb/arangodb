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

#pragma once
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/Supervision.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

namespace arangodb::test {

struct AgencyStateBuilder {
  AgencyStateBuilder() = default;

  auto setId(LogId id) -> AgencyStateBuilder& {
    _state.target.id = id;
    if (_state.plan) {
      _state.plan->id = id;
    }
    return *this;
  }

  auto setTargetParticipant(ParticipantId const& id) -> AgencyStateBuilder& {
    _state.target.participants[id];
    return *this;
  }
  template<typename... Vs>
  auto setTargetParticipants(Vs&&... vs) -> AgencyStateBuilder& {
    (setTargetParticipant(std::forward<Vs>(vs)), ...);
    return *this;
  };

  auto setTargetConfig(LogConfig config) -> AgencyStateBuilder& {
    _state.target.config = config;
    return *this;
  }

  auto setTargetLeader(std::optional<ParticipantId> leader)
      -> AgencyStateBuilder& {
    _state.target.leader = std::move(leader);
    return *this;
  }

  auto setTargetVersion(std::optional<std::uint64_t> version)
      -> AgencyStateBuilder& {
    _state.target.version = version;
    return *this;
  }

  auto setPlanGeneration(StateGeneration gen) -> AgencyStateBuilder& {
    makePlan().generation = gen;
    return *this;
  }

  auto setPlanParticipant(ParticipantId const& name, StateGeneration gen)
      -> AgencyStateBuilder& {
    makePlan().participants[name].generation = gen;
    return *this;
  }
  auto setPlanParticipant(ParticipantId const& name) -> AgencyStateBuilder& {
    makePlan().participants[name].generation = makePlan().generation;
    return *this;
  }
  template<typename... Vs>
  auto setPlanParticipants(Vs&&... vs) -> AgencyStateBuilder& {
    (setPlanParticipant(std::forward<Vs>(vs)), ...);
    return *this;
  }
  auto addPlanParticipant(ParticipantId const& name) -> AgencyStateBuilder& {
    makePlan().participants[name].generation = ++makePlan().generation;
    return *this;
  }

  auto makePlan() -> RSA::Plan& {
    if (!_state.plan.has_value()) {
      _state.plan.emplace();
      _state.plan->id = _state.target.id;
      _state.plan->generation = StateGeneration{1};
    }
    return _state.plan.value();
  }

  template<typename... Vs>
  auto setSnapshotCompleteFor(Vs&&... id) {
    ((makeCurrent().participants[id] =
          {_state.plan.value().participants.at(id).generation,
           SnapshotInfo{SnapshotStatus::kCompleted, SnapshotInfo::clock::now(),
                        std::nullopt}}),
     ...);
  }

  auto makeCurrent() -> RSA::Current& {
    if (!_state.current.has_value()) {
      _state.current.emplace();
    }
    return _state.current.value();
  }

  auto get() const noexcept -> RSA::State const& { return _state; }
  RSA::State _state;
};
}  // namespace arangodb::test
