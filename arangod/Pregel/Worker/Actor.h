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
#include "Pregel/Worker/Messages.h"
#include "Pregel/Conductor/Messages.h"
#include "fmt/core.h"

namespace arangodb::pregel {

struct WorkerState {
  actor::ActorPID conductor;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerState& x) {
  return f.object(x).fields(f.field("conductor", x.conductor));
}

template<typename Runtime>
struct WorkerHandler : actor::HandlerBase<Runtime, WorkerState> {
  auto operator()(WorkerStart start) -> std::unique_ptr<WorkerState> {
    LOG_TOPIC("cd696", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} started", this->self);
    // TODO when we get a message with a conductor PID
    // this->state->conductor = start.conductor;
    // this->template dispatch<ConductorMessages>(this->state->conductor,
    //                                            WorkerCreated{});
    return std::move(this->state);
  }

  auto operator()(actor::UnknownMessage unknown)
      -> std::unique_ptr<WorkerState> {
    LOG_TOPIC("7ee4d", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::ActorNotFound notFound)
      -> std::unique_ptr<WorkerState> {
    LOG_TOPIC("2d647", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - recieving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::NetworkError notFound)
      -> std::unique_ptr<WorkerState> {
    LOG_TOPIC("1c3d9", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<WorkerState> {
    LOG_TOPIC("8b81a", INFO, Logger::PREGEL)
        << "Worker Actor: Got unhandled message";
    LOG_TOPIC("f5bac", INFO, Logger::PREGEL) << fmt::format("{}", rest);
    return std::move(this->state);
  }
};

struct WorkerActor {
  using State = WorkerState;
  using Message = WorkerMessages;
  template<typename Runtime>
  using Handler = WorkerHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view { return "WorkerActor"; }
};
}  // namespace arangodb::pregel
