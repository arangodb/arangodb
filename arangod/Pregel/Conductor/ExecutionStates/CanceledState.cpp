////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "CanceledState.h"
#include "Pregel/Conductor/ExecutionStates/CleanedUpState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"

using namespace arangodb::pregel::conductor;

Canceled::Canceled(ConductorState& conductor) : conductor{conductor} {
  if (conductor.timing.loading.hasStarted() and
      not conductor.timing.loading.hasFinished()) {
    conductor.timing.loading.finish();
  }
  if (conductor.timing.computation.hasStarted() and
      not conductor.timing.computation.hasFinished()) {
    conductor.timing.computation.finish();
  }
  if (conductor.timing.storing.hasStarted() and
      not conductor.timing.storing.hasFinished()) {
    conductor.timing.storing.finish();
  }
  if (conductor.timing.total.hasStarted() and
      not conductor.timing.total.hasFinished()) {
    conductor.timing.total.finish();
  }
}

auto Canceled::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  auto messages =
      std::unordered_map<actor::ActorPID, worker::message::WorkerMessages>{};
  for (auto const& worker : conductor.workers) {
    messages.emplace(worker, worker::message::Cleanup{});
  }
  return messages;
}

auto Canceled::receive(actor::ActorPID sender,
                       message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<message::CleanupFinished>(message)) {
    return std::make_unique<FatalError>(conductor);
  }
  conductor.workers.erase(sender);
  if (conductor.workers.empty()) {
    return std::make_unique<CleanedUp>();
  }
  return std::nullopt;
};
