////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <string_view>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Inspection/InspectorBase.h"
#include "Actor/Actor.h"
#include "TrivialActor.h"

namespace arangodb::actor::test {

struct SpawnState {
  std::size_t called{};
  std::string state;
  bool operator==(const SpawnState&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnState& x) {
  return f.object(x).fields(f.field("called", x.called),
                            f.field("state", x.state));
}

namespace message {

struct SpawnStartMessage {};
template<typename Inspector>
auto inspect(Inspector& f, SpawnStartMessage& x) {
  return f.object(x).fields();
}

struct SpawnMessage {
  SpawnMessage() = default;
  SpawnMessage(std::string message) : message(std::move(message)) {}
  std::string message;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnMessage& x) {
  return f.object(x).fields(f.field("message", x.message));
}

struct SpawnActorMessage : std::variant<SpawnStartMessage, SpawnMessage> {
  using std::variant<SpawnStartMessage, SpawnMessage>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnActorMessage& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<SpawnStartMessage>("start"),
      arangodb::inspection::type<SpawnMessage>("spawn"));
}

}  // namespace message

template<typename Runtime>
struct SpawnHandler : HandlerBase<Runtime, SpawnState> {
  auto operator()(message::SpawnStartMessage msg)
      -> std::unique_ptr<SpawnState> {
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(message::SpawnMessage msg) -> std::unique_ptr<SpawnState> {
    this->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                       message::TrivialStart{});
    this->state->called++;
    this->state->state += msg.message;
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<SpawnState> {
    fmt::print(stderr, "Spawn actor: handles rest\n");
    return std::move(this->state);
  }
};

struct SpawnActor {
  using State = SpawnState;
  using Message = message::SpawnActorMessage;
  template<typename Runtime>
  using Handler = SpawnHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view { return "SpawnActor"; };
};
}  // namespace arangodb::actor::test

template<>
struct fmt::formatter<arangodb::actor::test::SpawnState>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::actor::test::message::SpawnMessage>
    : arangodb::inspection::inspection_formatter {};
