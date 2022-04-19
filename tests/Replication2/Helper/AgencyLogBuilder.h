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

struct AgencyLogBuilder {
  AgencyLogBuilder() = default;

  auto setId(LogId id) -> AgencyLogBuilder& {
    _log.target.id = id;
    if (_log.plan) {
      _log.plan->id = id;
    }
    return *this;
  }

  auto setTargetParticipant(ParticipantId const& id, ParticipantFlags flags)
      -> AgencyLogBuilder& {
    _log.target.participants[id] = flags;
    return *this;
  }

  auto setTargetConfig(LogConfig config) -> AgencyLogBuilder& {
    _log.target.config = config;
    return *this;
  }

  auto setTargetLeader(std::optional<ParticipantId> leader)
      -> AgencyLogBuilder& {
    _log.target.leader = std::move(leader);
    return *this;
  }

  auto setTargetVersion(std::optional<std::uint64_t> version)
      -> AgencyLogBuilder& {
    _log.target.version = version;
    return *this;
  }

  auto makeTerm() -> RLA::LogPlanTermSpecification& {
    auto& plan = makePlan();
    if (!plan.currentTerm.has_value()) {
      plan.currentTerm.emplace();
      plan.currentTerm->term = LogTerm{1};
      plan.currentTerm->config = _log.target.config;
    }
    return plan.currentTerm.value();
  }

  auto setPlanLeader(ParticipantId const& id) -> AgencyLogBuilder& {
    makeTerm().leader.emplace(id, RebootId{0});
    return *this;
  }

  auto setPlanParticipant(ParticipantId const& id, ParticipantFlags flags)
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

  auto makeCurrent() -> RLA::LogCurrent& {
    if (!_log.current.has_value()) {
      _log.current.emplace();
    }
    return _log.current.value();
  }

  auto get() const noexcept -> RLA::Log const& { return _log; }
  RLA::Log _log;
};
}  // namespace arangodb::test
