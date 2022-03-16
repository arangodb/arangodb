////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#pragma once
#include <memory>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {

struct EmptyAction {};

struct AddParticipantAction {
  ParticipantId participant;
  StateGeneration generation;

  void updateLogTarget(arangodb::replication2::agency::LogTarget& logTarget) {
    logTarget.participants[participant] =
        ParticipantFlags{.allowedInQuorum = false, .allowedAsLeader = false};
  }

  void updateStatePlan(agency::Plan& plan) {
    plan.participants[participant].generation = plan.generation;
    plan.generation.value += 1;
  }
};

struct AddStateToPlanAction {
  replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;

  void updateLogTarget(arangodb::replication2::agency::LogTarget& target) {
    target = logTarget;
  }

  void updateStatePlan(agency::Plan& plan) { plan = statePlan; }
};

struct UpdateParticipantFlagsAction {
  ParticipantId participant;
  ParticipantFlags flags;

  void updateLogTarget(arangodb::replication2::agency::LogTarget& target) {
    target.participants.at(participant) = flags;
  }
};

using Action = std::variant<EmptyAction, AddParticipantAction,
                            AddStateToPlanAction, UpdateParticipantFlagsAction>;

auto execute(LogId id, DatabaseID const& database, Action action,
             std::optional<agency::Plan> state,
             std::optional<replication2::agency::LogTarget> log,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_state
