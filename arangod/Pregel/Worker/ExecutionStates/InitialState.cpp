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

#include "InitialState.h"

#include <utility>
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "LoadingState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Initial<V, E, M>::Initial(actor::ActorPID self, WorkerState<V, E, M>& worker)
    : self(std::move(self)), worker{worker} {}

template<typename V, typename E, typename M>
auto Initial<V, E, M>::receive(actor::ActorPID const& sender,
                               worker::message::WorkerMessages const& message,
                               Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::PregelMessage>(message)) {
    dispatcher.dispatchSelf(message);

    return nullptr;
  }

  if (std::holds_alternative<worker::message::WorkerStart>(message)) {
    dispatcher.dispatchConductor(ResultT<conductor::message::WorkerCreated>{});
    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerStarted{});

    return nullptr;
  }

  if (std::holds_alternative<worker::message::LoadGraph>(message)) {
    dispatcher.dispatchSelf(message);

    return std::make_unique<Loading>(self, worker);
  }

  return std::make_unique<FatalError>();
}
