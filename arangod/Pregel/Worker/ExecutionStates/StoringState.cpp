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

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Storing<V, E, M>::Storing(actor::ActorPID self, WorkerState<V, E, M>& worker)
    : self(std::move(self)), worker{worker} {}

template<typename V, typename E, typename M>
auto Storing<V, E, M>::receive(actor::ActorPID const& sender,
                               worker::message::WorkerMessages const& message,
                               Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::Store>(message)) {
    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerStoringStarted{});

    auto graphStored = [this,
                        dispatcher]() -> ResultT<conductor::message::Stored> {
      try {
        auto storer = std::make_shared<GraphStorer<V, E>>(
            worker->config->executionNumber(), *worker->config->vocbase(),
            worker->config->parallelism(), worker->algorithm->inputFormat(),
            worker->config->globalShardIDs(),
            ActorStoringUpdate{.fn = dispatcher.dispatchStatus});
        storer->store(worker->magazine).get();

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

    dispatchConductor(graphStored());

    return std::make_unique<Stored>(self, worker);
  }

  return std::make_unique<FatalError>();
}
