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

#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Pregel/StatusMessages.h"
#include "fmt/core.h"

namespace arangodb::pregel {

struct StatusState {};
template<typename Inspector>
auto inspect(Inspector& f, StatusState& x) {
  return f.object(x).fields();
}

template<typename Runtime>
struct StatusHandler : actor::HandlerBase<Runtime, StatusState> {
  auto operator()(message::StatusStart start) -> std::unique_ptr<StatusState> {
    LOG_TOPIC("ea4f4", INFO, Logger::PREGEL)
        << fmt::format("Status Actor {} started", this->self);
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("eb6f2", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e31f6", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e87f3", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e9df2", INFO, Logger::PREGEL)
        << "Status Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct StatusActor {
  using State = StatusState;
  using Message = message::StatusMessages;
  template<typename Runtime>
  using Handler = StatusHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "Status Actor";
  }
};

}  // namespace arangodb::pregel
