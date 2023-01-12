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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Pregel/Actor/ActorPID.h"
#include "Pregel/Actor/HandlerBase.h"
#include "Inspection/InspectorBase.h"

namespace arangodb::pregel::actor::test {

namespace pong_actor {
struct Actor;

struct Start {};
template<typename Inspector>
auto inspect(Inspector& f, Start& x) {
  return f.object(x).fields();
}

struct Ping {
  std::string text;
};

template<typename Inspector>
auto inspect(Inspector& f, Ping& x) {
  return f.object(x).fields(f.field("text", x.text));
}

struct PingMessage : std::variant<Start, Ping> {
  using std::variant<Start, Ping>::variant;
};

template<typename Inspector>
auto inspect(Inspector& f, PingMessage& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<Start>("start"),
      arangodb::inspection::type<Ping>("ping"));
}

}  // namespace pong_actor

namespace ping_actor {

struct State {
  std::size_t called;
  std::string message;
  bool operator==(const State&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.object(x).fields(f.field("called", x.called),
                            f.field("message", x.message));
}

struct Start {
  ActorPID pongActor;
};
template<typename Inspector>
auto inspect(Inspector& f, Start& x) {
  return f.object(x).fields(f.field("pongActor", x.pongActor));
}

struct Pong {
  std::string text;
};
template<typename Inspector>
auto inspect(Inspector& f, Pong& x) {
  return f.object(x).fields(f.field("text", x.text));
}

struct PongMessage : std::variant<Start, Pong> {
  using std::variant<Start, Pong>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, PongMessage& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<Start>("start"),
      arangodb::inspection::type<Pong>("pong"));
}

template<typename Runtime>
struct Handler : HandlerBase<Runtime, State> {
  auto operator()(Start msg) -> std::unique_ptr<State> {
    this->template dispatch<pong_actor::PingMessage>(
        msg.pongActor, pong_actor::Ping{.text = "hello world"});
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(Pong msg) -> std::unique_ptr<State> {
    this->state->called++;
    this->state->message = msg.text;
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<State> {
    fmt::print(stderr, "PingActor: handles rest\n");
    return std::move(this->state);
  }
};

struct Actor {
  using State = State;
  template<typename Runtime>
  using Handler = Handler<Runtime>;
  using Message = PongMessage;
  static constexpr auto typeName() -> std::string_view { return "PingActor"; };
};

}  // namespace ping_actor

namespace pong_actor {

struct State {
  std::size_t called;
  bool operator==(const State&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.object(x).fields(f.field("called", x.called));
}

template<typename Runtime>
struct Handler : HandlerBase<Runtime, State> {
  auto operator()(Start msg) -> std::unique_ptr<State> {
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(Ping msg) -> std::unique_ptr<State> {
    this->template dispatch<ping_actor::Actor::Message>(
        this->sender, ping_actor::Pong{.text = msg.text});
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<State> {
    fmt::print(stderr, "PongActor: handles rest\n");
    return std::move(this->state);
  }
};

struct Actor {
  using State = State;
  template<typename Runtime>
  using Handler = Handler<Runtime>;
  using Message = PingMessage;
  static constexpr auto typeName() -> std::string_view { return "PongActor"; };
};

}  // namespace pong_actor

}  // namespace arangodb::pregel::actor::test

template<>
struct fmt::formatter<arangodb::pregel::actor::test::pong_actor::PingMessage>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::actor::test::ping_actor::State>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::actor::test::ping_actor::PongMessage>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::actor::test::pong_actor::State>
    : arangodb::inspection::inspection_formatter {};
