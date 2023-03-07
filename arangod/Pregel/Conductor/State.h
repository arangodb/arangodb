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
#include "Assertions/Assert.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/ExecutionStates/InitialState.h"
#include "Pregel/Conductor/ExecutionStates/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/ExecutionStatus.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"
#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb::pregel::conductor {

struct ConductorState {
  ConductorState(std::unique_ptr<IAlgorithm> algorithm,
                 ExecutionSpecifications specifications, TRI_vocbase_t& vocbase)
      : algorithm{std::move(algorithm)},
        specifications{std::move(specifications)},
        vocbaseGuard{vocbase} {}

  ExecutionTimings timing;
  std::unique_ptr<ExecutionState> executionState = std::make_unique<Initial>();
  uint64_t globalSuperstep = 0;
  ConductorStatus status;
  std::unordered_set<actor::ActorPID> workers;
  std::unique_ptr<IAlgorithm> algorithm;
  const ExecutionSpecifications specifications;
  const DatabaseGuard vocbaseGuard;
};

template<typename Inspector>
auto inspect(Inspector& f, ConductorState& x) {
  return f.object(x).fields(
      f.field("timing", x.timing),
      f.field("globalSuperstep", x.globalSuperstep),
      f.field("executionState", x.executionState->name()),
      f.field("status", x.status),
      // f.field("workers", x._workers), TODO make set inspectionable
      f.field("specifications", x.specifications));
}

}  // namespace arangodb::pregel::conductor

template<>
struct fmt::formatter<arangodb::pregel::conductor::ConductorState>
    : arangodb::inspection::inspection_formatter {};
