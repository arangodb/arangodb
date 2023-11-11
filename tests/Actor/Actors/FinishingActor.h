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

#include "Inspection/InspectorBase.h"
#include "Actor/Actor.h"
#include "TrivialActor.h"

namespace arangodb::actor::test {

struct FinishingState {
  bool operator==(const FinishingState&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, FinishingState& x) {
  return f.object(x).fields();
}

namespace message {

struct FinishingStart {};
template<typename Inspector>
auto inspect(Inspector& f, FinishingStart& x) {
  return f.object(x).fields();
}

struct FinishingFinish {
  bool finish;
};
template<typename Inspector>
auto inspect(Inspector& f, FinishingFinish& x) {
  return f.object(x).fields(f.field("finish", x.finish));
}

struct FinishingMessages : std::variant<FinishingStart, FinishingFinish> {
  using std::variant<FinishingStart, FinishingFinish>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, FinishingMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<FinishingStart>("start"),
      arangodb::inspection::type<FinishingFinish>("finish"));
}

}  // namespace message

template<typename Runtime>
struct FinishingHandler : HandlerBase<Runtime, FinishingState> {
  auto operator()(message::FinishingStart msg)
      -> std::unique_ptr<FinishingState> {
    return std::move(this->state);
  }

  auto operator()(message::FinishingFinish msg)
      -> std::unique_ptr<FinishingState> {
    this->finish();
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<FinishingState> {
    fmt::print(stderr, "Finishing actor: handles rest\n");
    return std::move(this->state);
  }
};

struct FinishingActor {
  using State = FinishingState;
  using Message = message::FinishingMessages;
  template<typename Runtime>
  using Handler = FinishingHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "FinishingActor";
  };
};
}  // namespace arangodb::actor::test

template<>
struct fmt::formatter<arangodb::actor::test::FinishingState>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::actor::test::message::FinishingMessages>
    : arangodb::inspection::inspection_formatter {};
