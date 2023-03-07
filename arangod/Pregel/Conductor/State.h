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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Actor/ActorPID.h"
#include "Pregel/Conductor/ExecutionStates/InitialState.h"
#include "Pregel/Conductor/ExecutionStates/State.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/ExecutionStatus.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel::conductor {

struct ConductorState {
  ConductorState(ExecutionSpecifications specifications, TRI_vocbase_t& vocbase)
      : _specifications{std::move(specifications)}, _vocbaseGuard{vocbase} {}

  ExecutionTimings _timing;
  uint64_t _globalSuperstep = 0;
  std::unique_ptr<ExecutionState> _executionState = std::make_unique<Initial>();
  // TODO how to update metrics in feature (needed for 'loading' and subsequent
  // states)
  ConductorStatus _status;
  std::vector<actor::ActorPID> _workers;
  const ExecutionSpecifications _specifications;
  const DatabaseGuard _vocbaseGuard;
};

template<typename Inspector>
auto inspect(Inspector& f, ConductorState& x) {
  return f.object(x).fields(
      f.field("timing", x._timing),
      f.field("globalSuperstep", x._globalSuperstep),
      f.field("executionState", x._executionState->name()),
      f.field("status", x._status), f.field("workers", x._workers),
      f.field("specifications", x._specifications));
}

}  // namespace arangodb::pregel::conductor

template<>
struct fmt::formatter<arangodb::pregel::conductor::ConductorState>
    : arangodb::inspection::inspection_formatter {};
