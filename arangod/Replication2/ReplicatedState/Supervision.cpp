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
#include <memory>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::replication2::replicated_state;

auto checkStateAdded(replication2::replicated_state::agency::State const& state)
    -> std::unique_ptr<Action> {
  if (!state.plan) {
    auto const spec = replication2::replicated_state::agency::Plan{};
    return std::make_unique<AddStateToPlanAction>(spec);
  } else {
    return std::make_unique<EmptyAction>();
  }
}

auto isEmptyAction(std::unique_ptr<Action>& action) {
  return (action->type() == Action::ActionType::EmptyAction);
}

auto checkReplicatedState(
    std::optional<arangodb::replication2::agency::Log> const& log,
    replication2::replicated_state::agency::State const& state)
    -> std::unique_ptr<Action> {

    LOG_DEVEL << " check replicated state";

  if (auto action = checkStateAdded(state); !isEmptyAction(action)) {
    return action;
  }

  return std::make_unique<EmptyAction>();
};
