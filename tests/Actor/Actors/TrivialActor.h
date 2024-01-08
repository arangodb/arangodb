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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <string_view>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Actor/Actor.h"

namespace arangodb::actor::test {

struct TrivialState {
  TrivialState(std::string state, std::size_t called)
      : state{std::move(state)}, called{called} {}
  TrivialState(std::string state) : state{std::move(state)}, called{0} {}
  TrivialState() = default;
  bool operator==(const TrivialState&) const = default;
  std::string state;
  std::size_t called{};
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialState& x) {
  return f.object(x).fields(f.field("state", x.state),
                            f.field("called", x.called));
}

namespace message {

struct TrivialStart {};
template<typename Inspector>
auto inspect(Inspector& f, TrivialStart& x) {
  return f.object(x).fields();
}

struct TrivialMessage {
  TrivialMessage() = default;
  // we intentionally make this message type move-only
  TrivialMessage(TrivialMessage&) = delete;
  TrivialMessage(TrivialMessage&&) = default;
  TrivialMessage& operator=(TrivialMessage&&) = default;
  TrivialMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialMessage& x) {
  return f.object(x).fields(f.field("store", x.store));
}

struct TrivialMessages : std::variant<TrivialStart, TrivialMessage> {
  using std::variant<TrivialStart, TrivialMessage>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<TrivialStart>("msg0"),
      arangodb::inspection::type<TrivialMessage>("msg1"));
}

}  // namespace message

template<typename Runtime>
struct TrivialHandler : HandlerBase<Runtime, TrivialState> {
  using ActorPID = typename Runtime::ActorPID;
  auto operator()(message::TrivialStart msg) -> std::unique_ptr<TrivialState> {
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(message::TrivialMessage msg)
      -> std::unique_ptr<TrivialState> {
    this->state->called++;
    this->state->state += msg.store;
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage<ActorPID> unknown)
      -> std::unique_ptr<TrivialState> {
    this->state->called++;
    this->state->state =
        fmt::format("sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound<ActorPID> notFound)
      -> std::unique_ptr<TrivialState> {
    this->state->called++;
    this->state->state =
        fmt::format("receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<TrivialState> {
    this->state->called++;
    this->state->state = fmt::format("network error: {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<TrivialState> {
    fmt::print(stderr, "TrivialActor: handles rest\n");
    return std::move(this->state);
  }
};

struct TrivialActor {
  using State = TrivialState;
  using Message = message::TrivialMessages;
  template<typename Runtime>
  using Handler = TrivialHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "TrivialActor";
  };
};
}  // namespace arangodb::actor::test

template<>
struct fmt::formatter<arangodb::actor::test::TrivialState>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::actor::test::message::TrivialMessages>
    : arangodb::inspection::inspection_formatter {};
