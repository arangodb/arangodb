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

#include "Pregel/Conductor/ExecutionStates/State.h"

namespace arangodb::pregel::conductor {

struct ConductorState;

/*
  This state is the final successful state if a pregel run is started with
  parameter store = false.

  In this state the pregel results can be queried via AQL:
  PregelFeature::getResults returns these results.
 */
struct AQLResultsAvailable : ExecutionState {
  AQLResultsAvailable(ConductorState& conductor);
  auto name() const -> std::string override { return "done"; }
  auto aqlResultsAvailable() const -> bool override { return true; }
  auto messages()
      -> std::unordered_map<actor::ActorPID,
                            worker::message::WorkerMessages> override;
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<std::unique_ptr<ExecutionState>> override;

  ConductorState& conductor;
};

}  // namespace arangodb::pregel::conductor
