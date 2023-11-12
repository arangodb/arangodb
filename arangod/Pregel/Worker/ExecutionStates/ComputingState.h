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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "State.h"
#include "Pregel/VertexComputation.h"

namespace arangodb::pregel::worker {
struct VerticesProcessed {
  std::unordered_map<actor::DistributedActorPID, uint64_t> sendCountPerActor;
  size_t activeCount;
};

template<typename V, typename E, typename M>
struct WorkerState;

template<typename V, typename E, typename M>
struct Computing : ExecutionState {
  explicit Computing(WorkerState<V, E, M>& worker);
  ~Computing() override = default;

  [[nodiscard]] auto name() const -> std::string override {
    return "computing";
  };
  auto receive(actor::DistributedActorPID const& sender,
               actor::DistributedActorPID const& self,
               message::WorkerMessages const& message, Dispatcher dispatcher)
      -> std::unique_ptr<ExecutionState> override;

  auto prepareGlobalSuperStep(message::RunGlobalSuperStep message) -> void;
  auto processVertices(Dispatcher const& dispatcher) -> VerticesProcessed;
  auto finishProcessing(VerticesProcessed verticesProcessed,
                        DispatchStatus const& dispatchStatus)
      -> conductor::message::GlobalSuperStepFinished;

  WorkerState<V, E, M>& worker;
  std::optional<std::chrono::steady_clock::time_point>
      isWaitingForAllMessagesSince;
  std::unique_ptr<InCache<M>> readCache = nullptr;
  std::unique_ptr<InCache<M>> writeCache = nullptr;
  size_t messageBatchSize = 500;
};
}  // namespace arangodb::pregel::worker
