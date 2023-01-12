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

#include "Pregel/Actor/Actor.h"

#include "Inspection/InspectorBase.h"

namespace arangodb::pregel::actor::test {

struct TrivialState {
  std::string state;
  std::size_t called{};
  bool operator==(const TrivialState&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialState& x) {
  return f.object(x).fields(f.field("state", x.state),
                            f.field("called", x.called));
}

struct TrivialMessage0 {};
template<typename Inspector>
auto inspect(Inspector& f, TrivialMessage0& x) {
  return f.object(x).fields();
}

struct TrivialMessage1 {
  TrivialMessage1() = default;
  TrivialMessage1(std::string value) : store(std::move(value)) {}
  std::string store;
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialMessage1& x) {
  return f.object(x).fields(f.field("store", x.store));
}

struct TrivialMessage : std::variant<TrivialMessage0, TrivialMessage1> {
  using std::variant<TrivialMessage0, TrivialMessage1>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, TrivialMessage& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<TrivialMessage0>("msg0"),
      arangodb::inspection::type<TrivialMessage1>("msg1"));
}

template<typename Runtime>
struct TrivialHandler : HandlerBase<Runtime, TrivialState> {
  auto operator()(TrivialMessage0 msg) -> std::unique_ptr<TrivialState> {
    this->state->called++;
    return std::move(this->state);
  }

  auto operator()(TrivialMessage1 msg) -> std::unique_ptr<TrivialState> {
    this->state->called++;
    this->state->state += msg.store;
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<TrivialState> {
    fmt::print(stderr, "TrivialActor: handles rest\n");
    return std::move(this->state);
  }
};

struct TrivialActor {
  using State = TrivialState;
  using Message = TrivialMessage;
  template<typename Runtime>
  using Handler = TrivialHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "TrivialActor";
  };
};
}  // namespace arangodb::pregel::actor::test

template<>
struct fmt::formatter<arangodb::pregel::actor::test::TrivialState>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::actor::test::TrivialMessage>
    : arangodb::inspection::inspection_formatter {};
