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

struct Computing : State {
  Conductor& conductor;
  Computing(Conductor& conductor);
  ~Computing();
  auto run() -> std::optional<std::unique_ptr<State>> override;
  auto canBeCanceled() -> bool override { return true; }
  auto name() const -> std::string override { return "running"; };
  auto isRunning() const -> bool override { return true; }
  auto getExpiration() const
      -> std::optional<std::chrono::system_clock::time_point> override {
    return std::nullopt;
  }

 private:
  using GlobalSuperStepPreparedFuture =
      futures::Future<std::vector<futures::Try<
          arangodb::ResultT<arangodb::pregel::GlobalSuperStepPrepared>>>>;
  auto _prepareGlobalSuperStep() -> GlobalSuperStepPreparedFuture;

  using GlobalSuperStepFinishedFuture = futures::Future<
      std::vector<futures::Try<ResultT<GlobalSuperStepFinished>>>>;
  auto _runGlobalSuperStep(bool activateAll) -> GlobalSuperStepFinishedFuture;
};

}  // namespace conductor
}  // namespace arangodb::pregel
