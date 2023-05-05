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

#include "ProducingResultsState.h"
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "Pregel/GraphStore/GraphVPackBuilderStorer.h"
#include "ResultsProducedState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
ProducingResults<V, E, M>::ProducingResults(actor::ActorPID self,
                                            WorkerState<V, E, M>& worker)
    : self(std::move(self)), worker{worker} {}

template<typename V, typename E, typename M>
auto ProducingResults<V, E, M>::receive(
    actor::ActorPID const& sender,
    worker::message::WorkerMessages const& message,
    DispatchStatus const& dispatchStatus,
    DispatchMetrics const& dispatchMetrics,
    DispatchConductor const& dispatchConductor,
    DispatchSelf const& dispatchSelf, DispatchOther const& dispatchOther,
    DispatchResult const& dispatchResult) -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::ProduceResults>(message)) {
    auto msg = std::get<worker::message::ProduceResults>(message);

    auto getResults = [this, msg]() -> ResultT<PregelResults> {
      std::function<void()> statusUpdateCallback = [] {
        // TODO GORDO-1584 send update to status actor
        // this->template dispatch<conductor::message::ConductorMessages>(
        //     worker->conductor,
        //     conductor::message::StatusUpdate{
        //         .executionNumber = worker->config->executionNumber(),
        //         .status = worker->observeStatus()});
      };
      try {
        auto storer = std::make_shared<GraphVPackBuilderStorer<V, E>>(
            msg.withID, worker->config, worker->algorithm->inputFormat(),
            std::move(statusUpdateCallback));
        storer->store(worker->magazine).get();

        return PregelResults{*storer->stealResult()};
      } catch (std::exception const& ex) {
        return Result{TRI_ERROR_INTERNAL,
                      fmt::format("caught exception when receiving results: {}",
                                  ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when receiving results"};
      }
    };
    auto results = getResults();
    dispatchResult(pregel::message::SaveResults{.results = {results}});
    dispatchConductor(
        pregel::conductor::message::ResultCreated{.results = {results}});

    return std::make_unique<ResultsProduced>(self, worker);
  }

  return std::make_unique<FatalError>();
}

template<typename V, typename E, typename M>
auto ProducingResults<V, E, M>::cancel(
    actor::ActorPID const& sender,
    worker::message::WorkerMessages const& message)
    -> std::unique_ptr<ExecutionState> {
  ExecutionState::cancel(sender, message);
}
