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

#include "LoadedState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "ComputingState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Loaded<V, E, M>::Loaded(actor::ActorPID self, WorkerState<V, E, M>& worker)
    : self(std::move(self)), worker{worker} {}

template<typename V, typename E, typename M>
auto Loaded<V, E, M>::receive(actor::ActorPID const& sender,
                              worker::message::WorkerMessages const& message,
                              DispatchStatus const& dispatchStatus,
                              DispatchMetrics const& dispatchMetrics,
                              DispatchConductor const& dispatchConductor,
                              DispatchSelf const& dispatchSelf,
                              DispatchOther const& dispatchOther)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::PregelMessage>(message)) {
    dispatchSelf(message);

    return nullptr;
  }

  if (std::holds_alternative<worker::message::RunGlobalSuperStep>(message)) {
    dispatchSelf(message);

    return std::make_unique<Computing>(self, worker);
  }

  return std::make_unique<FatalError>();
}

template<typename V, typename E, typename M>
auto Loaded<V, E, M>::cancel(actor::ActorPID const& sender,
                             worker::message::WorkerMessages const& message)
    -> std::unique_ptr<ExecutionState> {
  ExecutionState::cancel(sender, message);
}
