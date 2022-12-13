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

#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Actor/Actor.h"

namespace arangodb::pregel::actor::test {

struct TrivialState {
  std::string state;
  std::size_t called{};
  bool operator==(const TrivialState&) const = default;
};

struct TrivialMessage0 {};

struct TrivialMessage1 {
  TrivialMessage1(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct TrivialHandler : HandlerBase<TrivialState> {
  auto operator()(TrivialMessage0 msg) -> std::unique_ptr<TrivialState> {
    state->called++;
    return std::move(state);
  }

  auto operator()(TrivialMessage1 msg) -> std::unique_ptr<TrivialState> {
    state->called++;
    state->state += msg.store;
    return std::move(state);
  }
};

struct TrivialActor {
  using State = TrivialState;
  using Message = std::variant<TrivialMessage0, TrivialMessage1>;
  using Handler = TrivialHandler;
};
}  // namespace arangodb::pregel::actor::test
