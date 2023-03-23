////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>
#include <variant>
#include "Actor/HandlerBase.h"
#include "Pregel/Conductor/ExecutionStates/CreateWorkersState.h"
#include "Pregel/SpawnActor.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/Conductor/State.h"
#include "fmt/core.h"

namespace arangodb::pregel::conductor {

template<typename Runtime>
struct ConductorHandler : actor::HandlerBase<Runtime, ConductorState> {
  auto operator()(message::ConductorStart start)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("5adb0", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor {} started with state {}", this->self, *this->state);
    this->state->executionState = std::make_unique<CreateWorkers>(*this->state);
    /*
       CreateWorkers is a special state because it creates the workers instead
       of just sending messages to them. Therefore we cannot use the messages
       fct of the ExecutionState but need to call the special messagesToServers
       function which is specific to the CreateWorkers state only
    */
    auto createWorkers =
        static_cast<CreateWorkers*>(this->state->executionState.get());
    auto messages = createWorkers->messagesToServers();
    for (auto& [server, message] : messages) {
      this->dispatch(
          this->state->spawnActor,
          pregel::message::SpawnMessages{
              pregel::message::SpawnWorker{.destinationServer = server,
                                           .conductor = this->self,
                                           .message = message}});
    }
    return std::move(this->state);
  }

  auto operator()(ResultT<message::WorkerCreated> start)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("17915", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Worker {} was created", this->sender);
    auto newExecutionState =
        this->state->executionState->receive(this->sender, std::move(start));
    if (newExecutionState.has_value()) {
      changeState(std::move(newExecutionState.value()));
      sendMessages();
    }
    return std::move(this->state);
  }

  auto operator()(ResultT<message::GraphLoaded> start)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("1791c", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor: Graph was loaded in worker {}", this->sender);
    auto newExecutionState =
        this->state->executionState->receive(this->sender, std::move(start));
    if (newExecutionState.has_value()) {
      changeState(std::move(newExecutionState.value()));
      sendMessages();
    }
    return std::move(this->state);
  }

  auto operator()(ResultT<message::GlobalSuperStepFinished> message)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("543aa", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor: Global super step finished on worker {}",
        this->sender);
    return std::move(this->state);
  }

  auto operator()(message::StatusUpdate message)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("f89db", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor: Received status update from worker {}: {}",
        this->sender, inspection::json(message));
    return std::move(this->state);
  }

  auto operator()(message::ResultCreated msg)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("e1791", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor: Received results from {}", this->sender);

    auto state = this->state->executionState->receive(this->sender, msg);
    if (state.has_value()) {
      changeState(std::move(state.value()));
    }

    this->template dispatch<pregel::message::ResultMessages>(
        this->state->resultActor,
        pregel::message::AddResults{
            .results = msg.results,
            .receivedAllResults =
                this->state->executionState->aqlResultsAvailable()});
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("d1791", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Error - sent unknown message to {}",
                       unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("ea585", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Error - receiving actor {} not found",
                       notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("866d8", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("7ae0f", INFO, Logger::PREGEL)
        << "Conductor Actor: Got unhandled message";
    return std::move(this->state);
  }

  auto changeState(std::unique_ptr<ExecutionState> newState) -> void {
    this->state->executionState = std::move(newState);
    LOG_TOPIC("e3b0c", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Execution state changed to {}",
                       this->state->executionState->name());
  }

  auto sendMessages() -> void {
    auto messages = this->state->executionState->messages();
    for (auto const& [worker, message] : messages) {
      this->dispatch(worker, message);
    }
  }
};

}  // namespace arangodb::pregel::conductor
