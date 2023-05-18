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

#include "StoringState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "Pregel/GraphStore/GraphStorer.h"
#include "StoredState.h"
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
Storing<V, E, M>::Storing(WorkerState<V, E, M>& worker) : worker{worker} {}

template<typename V, typename E, typename M>
auto Storing<V, E, M>::receive(actor::ActorPID const& sender,
                               actor::ActorPID const& self,
                               worker::message::WorkerMessages const& message,
                               Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::Store>(message)) {
    LOG_TOPIC("980d9", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is storing", self);

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerStoringStarted{});

    auto graphStored = [this,
                        dispatcher]() -> ResultT<conductor::message::Stored> {
      try {
        auto storer = std::make_shared<GraphStorer<V, E>>(
            worker.config->executionNumber(), *worker.config->vocbase(),
            worker.config->parallelism(), worker.algorithm->inputFormat(),
            worker.config->globalShardIDs(),
            ActorStoringUpdate{.fn = dispatcher.dispatchStatus});
        storer->store(worker.magazine).get();

        return conductor::message::Stored{};
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception when storing graph: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when storing graph"};
      }
    };
    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerStoringFinished{});
    dispatcher.dispatchConductor(graphStored());

    return std::make_unique<Stored>();
  }

  return std::make_unique<FatalError>();
}

// template types to create
template struct arangodb::pregel::worker::Storing<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::worker::Storing<uint64_t, uint8_t, uint64_t>;
template struct arangodb::pregel::worker::Storing<float, float, float>;
template struct arangodb::pregel::worker::Storing<double, float, double>;
template struct arangodb::pregel::worker::Storing<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::worker::Storing<uint64_t, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Storing<algos::WCCValue, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Storing<algos::SCCValue, int8_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Storing<algos::HITSValue, int8_t,
                                                  SenderMessage<double>>;
template struct arangodb::pregel::worker::Storing<
    algos::HITSKleinbergValue, int8_t, SenderMessage<double>>;
template struct arangodb::pregel::worker::Storing<algos::ECValue, int8_t,
                                                  HLLCounter>;
template struct arangodb::pregel::worker::Storing<algos::DMIDValue, float,
                                                  DMIDMessage>;
template struct arangodb::pregel::worker::Storing<algos::LPValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::worker::Storing<algos::SLPAValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::worker::Storing<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
