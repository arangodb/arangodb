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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "State.h"
#include <set>

namespace arangodb::pregel::conductor {

struct ConductorState;

/*
  This state produces the pregel results that can be queried via AQL. It is only
  reached if a pregel run ist started with parameter store = false.

  It produces pregel results on each worker and combines all of these on
  the conductor.
 */
struct ProduceAQLResults : ExecutionState {
  ProduceAQLResults(ConductorState& conductor);
  // Discussable Note: To prevent API state change we're using "storing" as the
  // defined name here. Better: Use the real name of that state.
  auto name() const -> std::string override { return "storing"; };
  auto messages()
      -> std::unordered_map<actor::ActorPID,
                            worker::message::WorkerMessages> override;
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;
  auto cancel(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;

 private:
  ConductorState& conductor;
  std::unordered_set<actor::ActorPID> respondedWorkers;
  size_t responseCount{0};
};

}  // namespace arangodb::pregel::conductor
