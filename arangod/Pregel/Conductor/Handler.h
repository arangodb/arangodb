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
#include "Pregel/Conductor/ExecutionStates/CreateWorkers.h"
#include "Pregel/SpawnActor.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/Conductor/State.h"
#include "fmt/core.h"

namespace arangodb::pregel::conductor {

template<typename Runtime>
struct ConductorHandler : actor::HandlerBase<Runtime, ConductorState> {
  void spawnActorOnServer(actor::ServerID server,
                          pregel::message::SpawnMessages msg) {
    if (server == this->self.server) {
      this->template spawn<SpawnActor>(std::make_unique<SpawnState>(), msg);
    } else {
      this->dispatch(
          actor::ActorPID{
              .server = server, .database = this->self.database, .id = {0}},
          msg);
    }
  }

  auto operator()(message::ConductorStart start)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("5adb0", INFO, Logger::PREGEL) << fmt::format(
        "Conductor Actor {} started with state {}", this->self, *this->state);
    this->state->_executionState =
        std::make_unique<CreateWorkers>(*this->state);
    /*
       CreateWorkers is a special state because it creates the workers instead
       of just sending messages to them. Therefore we cannot use the message fct
       of the ExecutionState but need to call the special messages function
       which is specific to the CreateWorkers state only
    */
    auto createWorkers =
        static_cast<CreateWorkers*>(this->state->_executionState.get());
    auto messages = createWorkers->messages();
    for (auto& [server, message] : messages) {
      spawnActorOnServer(
          server, pregel::message::SpawnMessages{pregel::message::SpawnWorker{
                      .conductor = this->self, .message = message}});
    }
    return std::move(this->state);
  }

  auto operator()(ResultT<message::WorkerCreated> start)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("17915", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Worker {} was created", this->sender);
    auto newExecutionState =
        this->state->_executionState->receive(this->sender, std::move(start));
    if (newExecutionState.has_value()) {
      changeState(std::move(newExecutionState.value()));
      sendMessageToWorkers();
    }
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
    this->state->_executionState = std::move(newState);
    LOG_TOPIC("e3b0c", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Execution state changed to {}",
                       this->state->_executionState->name());
  }

  auto sendMessageToWorkers() -> void {
    auto message = this->state->_executionState->message();
    // TODO activate when loading state is implemented (GORDO-1548)
    // for (auto& worker : this->state->_workers) {
    //   this->dispatch(worker, message);
    // }
  }
};

}  // namespace arangodb::pregel::conductor
