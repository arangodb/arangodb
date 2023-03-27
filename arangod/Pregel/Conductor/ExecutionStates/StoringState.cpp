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

#include "StoringState.h"
#include "Pregel/Conductor/ExecutionStates/DoneState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"

using namespace arangodb::pregel::conductor;

Storing::Storing(ConductorState& conductor) : conductor{conductor} {
  conductor.timing.storing.start();
  // TODO GORDO-1510
  // conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_add(1);
}

Storing::~Storing() {
  conductor.timing.storing.finish();
  // TODO GORDO-1510
  // conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_sub(1);
}

auto Storing::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  auto out =
      std::unordered_map<actor::ActorPID, worker::message::WorkerMessages>();
  for (auto const& worker : conductor.workers) {
    out.emplace(worker, worker::message::Store{});
  }
  return out;
}

auto Storing::receive(actor::ActorPID sender,
                      message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::Stored>>(message)) {
    return std::make_unique<FatalError>(conductor);
  }
  auto stored = std::get<ResultT<message::Stored>>(message);
  if (not stored.ok()) {
    return std::make_unique<FatalError>(conductor);
  }
  respondedWorkers.emplace(sender);

  if (respondedWorkers == conductor.workers) {
    return std::make_unique<Done>(conductor);
  }

  return std::nullopt;
};
