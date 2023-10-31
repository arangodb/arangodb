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

#include <thread>

#include "Actor/Actor.h"

namespace arangodb::pregel::actor::test {

struct EgressData {
  auto set(std::string newContent) -> void {
    content = std::move(newContent);
    finished.store(true);
  }
  auto get() -> std::optional<std::string> {
    if (not finished.load()) {
      return std::nullopt;
    }
    return content;
  }
  std::atomic<bool> finished;
  std::string content;
};

struct EgressState {
  bool operator==(const EgressState&) const = default;
  EgressState() : data{std::make_shared<EgressData>()} {};
  std::shared_ptr<EgressData> data;
};
template<typename Inspector>
auto inspect(Inspector& f, EgressState& x) {
  return f.object(x).fields();
}

namespace message {

struct EgressStart {};
template<typename Inspector>
auto inspect(Inspector& f, EgressStart& x) {
  return f.object(x).fields();
}

struct EgressSet {
  std::string data;
};
template<typename Inspector>
auto inspect(Inspector& f, EgressSet& x) {
  return f.object(x).fields(f.field("store", x.data));
}

struct EgressMessages : std::variant<EgressStart, EgressSet> {
  using std::variant<EgressStart, EgressSet>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, EgressMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<EgressStart>("start"),
      arangodb::inspection::type<EgressSet>("set"));
}

}  // namespace message

template<typename Runtime>
struct EgressHandler : HandlerBase<Runtime, EgressState> {
  auto operator()(message::EgressStart msg) -> std::unique_ptr<EgressState> {
    return std::move(this->state);
  }

  auto operator()(message::EgressSet msg) -> std::unique_ptr<EgressState> {
    this->state->data->set(msg.data);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<EgressState> {
    fmt::print(stderr, "EgressActor: handles rest\n");
    return std::move(this->state);
  }
};

struct EgressActor {
  using State = EgressState;
  using Message = message::EgressMessages;
  template<typename Runtime>
  using Handler = EgressHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "EgressActor";
  };
};
}  // namespace arangodb::pregel::actor::test
