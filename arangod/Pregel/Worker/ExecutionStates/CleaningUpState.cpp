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

#include "CleaningUpState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "StoredState.h"
#include "CleanedUpState.h"
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
CleaningUp<V, E, M>::CleaningUp(WorkerState<V, E, M>& worker)
    : worker{worker} {}

template<typename V, typename E, typename M>
auto CleaningUp<V, E, M>::receive(
    actor::ActorPID const& sender, actor::ActorPID const& self,
    worker::message::WorkerMessages const& message, Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::Cleanup>(message)) {
    dispatcher.dispatchSpawn(pregel::message::SpawnCleanup{});
    dispatcher.dispatchConductor(pregel::conductor::message::CleanupFinished{});
    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerFinished{});

    return std::make_unique<CleanedUp>();
  }

  return std::make_unique<FatalError>();
}

// template types to create
template struct arangodb::pregel::worker::CleaningUp<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::worker::CleaningUp<uint64_t, uint8_t,
                                                     uint64_t>;
template struct arangodb::pregel::worker::CleaningUp<float, float, float>;
template struct arangodb::pregel::worker::CleaningUp<double, float, double>;
template struct arangodb::pregel::worker::CleaningUp<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::worker::CleaningUp<uint64_t, uint64_t,
                                                     SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::CleaningUp<algos::WCCValue, uint64_t,
                                                     SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::CleaningUp<algos::SCCValue, int8_t,
                                                     SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::CleaningUp<algos::HITSValue, int8_t,
                                                     SenderMessage<double>>;
template struct arangodb::pregel::worker::CleaningUp<
    algos::HITSKleinbergValue, int8_t, SenderMessage<double>>;
template struct arangodb::pregel::worker::CleaningUp<algos::ECValue, int8_t,
                                                     HLLCounter>;
template struct arangodb::pregel::worker::CleaningUp<algos::DMIDValue, float,
                                                     DMIDMessage>;
template struct arangodb::pregel::worker::CleaningUp<algos::LPValue, int8_t,
                                                     uint64_t>;
template struct arangodb::pregel::worker::CleaningUp<algos::SLPAValue, int8_t,
                                                     uint64_t>;
template struct arangodb::pregel::worker::CleaningUp<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
