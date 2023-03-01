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
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Pregel/Worker/Actor.h"
#include "Pregel/SpawnMessages.h"
#include "fmt/core.h"

namespace arangodb::pregel {

struct SpawnState {};
template<typename Inspector>
auto inspect(Inspector& f, SpawnState& x) {
  return f.object(x).fields();
}

template<typename Runtime>
struct SpawnHandler : actor::HandlerBase<Runtime, SpawnState> {
  auto operator()(message::SpawnStart start) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("4a414", INFO, Logger::PREGEL)
        << fmt::format("Spawn Actor {} started", this->self);
    return std::move(this->state);
  }

  auto operator()(message::SpawnWorker msg) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("2452c", INFO, Logger::PREGEL)
        << "Spawn Actor: Spawn worker actor";
    this->template spawn<worker::WorkerActor>(
        std::make_unique<worker::WorkerState>(
            msg.conductor, msg.message.executionSpecifications,
            msg.message.collectionSpecifications),
        worker::message::WorkerStart{});
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("7b602", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("03156", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("a87b3", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("89d72", INFO, Logger::PREGEL)
        << "Spawn Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct SpawnActor {
  using State = SpawnState;
  using Message = message::SpawnMessages;
  template<typename Runtime>
  using Handler = SpawnHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view { return "Spawn Actor"; }
};

}  // namespace arangodb::pregel
