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

#include "Supervision.h"

#include "Replication2/ReplicatedState/SupervisionAction.h"
#include <memory>

#include "Logger/LogMacros.h"

using namespace arangodb;

namespace arangodb::replication2::replicated_state {

auto checkStateAdded(replication2::replicated_state::agency::State const& state)
    -> std::unique_ptr<Action> {
  auto id = state.target.id;

  if (!state.plan) {
    auto participants =
        arangodb::replication2::agency::LogTarget::Participants{};

    auto statePlan = replication2::replicated_state::agency::Plan{
        .id = state.target.id,
        .generation = StateGeneration{1},
        .properties = state.target.properties};

    auto logTarget =
        replication2::agency::LogTarget(id, participants, state.target.config);

    for (auto const& [participantId, _] : state.target.participants) {
      logTarget.participants.emplace(participantId, ParticipantFlags{});
      statePlan.participants.emplace(
          participantId,
          agency::Plan::Participant{.generation = StateGeneration{1}});
    }

    return std::make_unique<AddStateToPlanAction>(logTarget, statePlan);
  } else {
    return std::make_unique<EmptyAction>();
  }
}

auto checkStateLog(
    std::optional<arangodb::replication2::agency::Log> const& log,
    replication2::replicated_state::agency::State const& state)
    -> std::unique_ptr<Action> {
  if (!log) {
    return std::make_unique<CreateLogForStateAction>(
        replication2::replicated_state::agency::Plan{});
  } else {
    return std::make_unique<EmptyAction>();
  }
}

auto isEmptyAction(std::unique_ptr<Action>& action) {
  return action->type() == Action::ActionType::EmptyAction;
}

auto checkReplicatedState(
    std::optional<arangodb::replication2::agency::Log> const& log,
    replication2::replicated_state::agency::State const& state)
    -> std::unique_ptr<Action> {
  if (auto action = checkStateAdded(state); !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkStateLog(log, state); !isEmptyAction(action)) {
  }

  return std::make_unique<EmptyAction>();
};

}  // namespace arangodb::replication2::replicated_state
