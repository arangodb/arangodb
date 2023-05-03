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

#include "LoadingState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "Pregel/GraphStore/GraphLoader.h"
#include "LoadedState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Loading<V, E, M>::Loading(WorkerState<V, E, M>& worker) : worker{worker} {}

template<typename V, typename E, typename M>
auto Loading<V, E, M>::receive(actor::ActorPID const& sender,
                               worker::message::WorkerMessages const& message,
                               DispatchStatus const& dispatchStatus,
                               DispatchMetrics const& dispatchMetrics,
                               DispatchConductor const& dispatchConductor,
                               DispatchSelf const& dispatchSelf)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::LoadGraph>(message)) {
    auto msg = std::get<worker::message::LoadGraph>(message);
    worker.outCache->setResponsibleActorPerShard(msg.responsibleActorPerShard);

    dispatchMetrics(arangodb::pregel::metrics::message::WorkerLoadingStarted{});

    auto graphLoaded =
        [this, dispatchStatus]() -> ResultT<conductor::message::GraphLoaded> {
      try {
        auto loader = GraphLoader(
            worker.config, worker.algorithm->inputFormat(),
            ActorLoadingUpdate{
                .fn = [this, dispatchStatus](
                          pregel::message::GraphLoadingUpdate update) -> void {
                  dispatchStatus(update);
                }});
        worker.magazine = loader.load().get();

        LOG_TOPIC("5206c", WARN, Logger::PREGEL)
            << fmt::format("Worker {} has finished loading.", this->self);
        return {conductor::message::GraphLoaded{
            .executionNumber = worker.config->executionNumber(),
            .vertexCount = worker.magazine.numberOfVertices(),
            .edgeCount = worker.magazine.numberOfEdges()}};
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception when loading graph: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when loading graph"};
      }
    };

    dispatchConductor(graphLoaded());
    dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerLoadingFinished{});

    return std::make_unique<Loaded>(worker);
  } else {
    return std::make_unique<FatalError>();
  }
}

template<typename V, typename E, typename M>
auto Loading<V, E, M>::cancel(actor::ActorPID const& sender,
                              worker::message::WorkerMessages const& message)
    -> std::unique_ptr<ExecutionState> {
  ExecutionState::cancel(sender, message);
}
