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

#include "StoredState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "ComputingState.h"
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
Stored<V, E, M>::Stored(WorkerState<V, E, M>& worker) : worker{worker} {}

template<typename V, typename E, typename M>
auto Stored<V, E, M>::receive(actor::ActorPID const& sender,
                              actor::ActorPID const& self,
                              worker::message::WorkerMessages const& message,
                              Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::Cleanup>(message)) {
    dispatcher.dispatchSelf(message);

    return nullptr;
  }

  return std::make_unique<FatalError>();
}

// template types to create
template struct arangodb::pregel::worker::Stored<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::worker::Stored<uint64_t, uint8_t, uint64_t>;
template struct arangodb::pregel::worker::Stored<float, float, float>;
template struct arangodb::pregel::worker::Stored<double, float, double>;
template struct arangodb::pregel::worker::Stored<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::worker::Stored<uint64_t, uint64_t,
                                                 SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Stored<algos::WCCValue, uint64_t,
                                                 SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Stored<algos::SCCValue, int8_t,
                                                 SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Stored<algos::HITSValue, int8_t,
                                                 SenderMessage<double>>;
template struct arangodb::pregel::worker::Stored<algos::HITSKleinbergValue,
                                                 int8_t, SenderMessage<double>>;
template struct arangodb::pregel::worker::Stored<algos::ECValue, int8_t,
                                                 HLLCounter>;
template struct arangodb::pregel::worker::Stored<algos::DMIDValue, float,
                                                 DMIDMessage>;
template struct arangodb::pregel::worker::Stored<algos::LPValue, int8_t,
                                                 uint64_t>;
template struct arangodb::pregel::worker::Stored<algos::SLPAValue, int8_t,
                                                 uint64_t>;
template struct arangodb::pregel::worker::Stored<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
