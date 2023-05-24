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
#include "Pregel/Algos/WCC/WCCValue.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDMessage.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Loading<V, E, M>::Loading(WorkerState<V, E, M>& worker) : worker{worker} {}

template<typename V, typename E, typename M>
auto Loading<V, E, M>::receive(actor::ActorPID const& sender,
                               actor::ActorPID const& self,
                               worker::message::WorkerMessages const& message,
                               Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::LoadGraph>(message)) {
    LOG_TOPIC("cd69c", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is loading", self);

    auto msg = std::get<worker::message::LoadGraph>(message);
    worker.responsibleActorPerShard = msg.responsibleActorPerShard;

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerLoadingStarted{});

    auto graphLoaded = [this, dispatcher,
                        self]() -> ResultT<conductor::message::GraphLoaded> {
      try {
        auto loader = std::make_shared<GraphLoader<V, E>>(
            worker.config, worker.algorithm->inputFormat(),
            ActorLoadingUpdate{
                .fn = [dispatcher](pregel::message::GraphLoadingUpdate update)
                    -> void { dispatcher.dispatchStatus(update); }});
        worker.magazine = loader->load().get();

        LOG_TOPIC("5206c", WARN, Logger::PREGEL)
            << fmt::format("Worker {} has finished loading.", self);
        return {conductor::message::GraphLoaded{
            .executionNumber = worker.config->executionNumber(),
            .vertexCount = worker.magazine->numberOfVertices(),
            .edgeCount = worker.magazine->numberOfEdges()}};
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception when loading graph: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when loading graph"};
      }
    };

    dispatcher.dispatchConductor(graphLoaded());
    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerLoadingFinished{});

    return std::make_unique<Loaded<V, E, M>>(worker);
  }

  return std::make_unique<FatalError>();
}

// template types to create
template struct arangodb::pregel::worker::Loading<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::worker::Loading<uint64_t, uint8_t, uint64_t>;
template struct arangodb::pregel::worker::Loading<float, float, float>;
template struct arangodb::pregel::worker::Loading<double, float, double>;
template struct arangodb::pregel::worker::Loading<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::worker::Loading<uint64_t, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Loading<algos::WCCValue, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Loading<algos::SCCValue, int8_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Loading<algos::HITSValue, int8_t,
                                                  SenderMessage<double>>;
template struct arangodb::pregel::worker::Loading<
    algos::HITSKleinbergValue, int8_t, SenderMessage<double>>;
template struct arangodb::pregel::worker::Loading<algos::ECValue, int8_t,
                                                  HLLCounter>;
template struct arangodb::pregel::worker::Loading<algos::DMIDValue, float,
                                                  DMIDMessage>;
template struct arangodb::pregel::worker::Loading<algos::LPValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::worker::Loading<algos::SLPAValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::worker::Loading<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
