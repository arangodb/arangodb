////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <ostream>
#include <string_view>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Inspection/InspectorBase.h"
#include "Actor/Actor.h"
#include "TrivialActor.h"

namespace arangodb::actor::test {

struct MonitoringState {
  std::vector<std::pair<ActorID, ExitReason>> deadActors;
  bool operator==(const MonitoringState&) const = default;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, MonitoringState& x) {
    return f.object(x).fields(f.field("deadActors", x.deadActors));
  }

  friend inline std::ostream& operator<<(std::ostream& stream,
                                         MonitoringState state) {
    stream << inspection::json(state);
    return stream;
  }
};

namespace message {

struct DummyMessage {};
template<typename Inspector>
auto inspect(Inspector& f, DummyMessage& x) {
  return f.object(x).fields();
}

struct MonitoringMessages : std::variant<DummyMessage> {
  using std::variant<DummyMessage>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, MonitoringMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<DummyMessage>("dummy"));
}

}  // namespace message

template<typename Runtime>
struct MonitoringHandler : HandlerBase<Runtime, MonitoringState> {
  auto operator()(actor::message::ActorDown<typename Runtime::ActorPID> msg)
      -> std::unique_ptr<MonitoringState> {
    this->state->deadActors.push_back({msg.actor.id, msg.reason});
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<MonitoringState> {
    fmt::print(stderr, "Monitoring actor: handles rest\n");
    return std::move(this->state);
  }
};

struct MonitoringActor {
  using State = MonitoringState;
  using Message = message::MonitoringMessages;
  template<typename Runtime>
  using Handler = MonitoringHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "MonitoringActor";
  };
};
}  // namespace arangodb::actor::test
