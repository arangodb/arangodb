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
#include "Actor/HandlerBase.h"
#include "Pregel/Worker/Actor.h"
#include "Pregel/Conductor/Messages.h"
#include "fmt/core.h"

namespace arangodb::pregel {

struct ConductorState {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorState& x) {
  return f.object(x).fields();
}

template<typename Runtime>
struct ConductorHandler : actor::HandlerBase<Runtime, ConductorState> {
  auto operator()(ConductorStart start) -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("56db0", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor {} started", this->self);
    // TODO call State::initializeWorkers in here
    // and then spawn actors from here
    return std::move(this->state);
  }

  auto operator()(WorkerCreated start) -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("17915", INFO, Logger::PREGEL)
        << "Conductor Actor: Worker was created";
    return std::move(this->state);
  }

  auto operator()(actor::UnknownMessage unknown)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("d1791", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Error - sent unknown message to {}",
                       unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::ActorNotFound notFound)
      -> std::unique_ptr<ConductorState> {
    LOG_TOPIC("ea585", INFO, Logger::PREGEL)
        << fmt::format("Conductor Actor: Error - recieving actor {} not found",
                       notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::NetworkError notFound)
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
};

struct ConductorActor {
  using State = ConductorState;
  using Message = ConductorMessages;
  template<typename Runtime>
  using Handler = ConductorHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "Conductor Actor";
  }
};

}  // namespace arangodb::pregel
