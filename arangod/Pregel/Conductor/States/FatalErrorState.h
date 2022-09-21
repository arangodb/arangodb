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

#include "State.h"

namespace arangodb::pregel {

class Conductor;

namespace conductor {

struct FatalError : State {
  std::chrono::system_clock::time_point expiration;
  Conductor& conductor;
  FatalError(Conductor& conductor);
  ~FatalError() = default;
  auto run() -> std::optional<std::unique_ptr<State>> override {
    return std::nullopt;
  };
  auto canBeCanceled() -> bool override { return true; }
  auto getResults(bool withId) -> ResultT<PregelResults> override;
  auto name() const -> std::string override { return "fatal error"; };
  auto isRunning() const -> bool override { return false; }
  auto getExpiration() const
      -> std::optional<std::chrono::system_clock::time_point> override {
    return expiration;
  };

 private:
  using ResultsFuture = futures::Future<std::vector<
      futures::Try<arangodb::ResultT<arangodb::pregel::PregelResults>>>>;
  auto _results(bool withId) -> ResultsFuture;
};

}  // namespace conductor
}  // namespace arangodb::pregel
