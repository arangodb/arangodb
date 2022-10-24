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

#include "Pregel/Conductor/WorkerApi.h"
#include "State.h"

namespace arangodb::pregel {

class Conductor;

namespace conductor {

struct Computing : State {
  Conductor& conductor;
  Computing(Conductor& conductor,
            WorkerApi<GlobalSuperStepFinished>&& workerApi);
  ~Computing();
  auto run() -> std::optional<std::unique_ptr<State>> override;
  auto receive(MessagePayload message)
      -> std::optional<std::unique_ptr<State>> override;
  auto cancel() -> std::optional<std::unique_ptr<State>> override;
  auto name() const -> std::string override { return "running"; };
  auto isRunning() const -> bool override { return true; }
  auto getExpiration() const
      -> std::optional<std::chrono::system_clock::time_point> override {
    return std::nullopt;
  }

 private:
  WorkerApi<GlobalSuperStepFinished> _workerApi;
  std::unordered_map<ServerID, uint64_t> _sendCountPerServer;
  auto _runGlobalSuperStepCommand() const
      -> std::unordered_map<ServerID, RunGlobalSuperStep>;
  auto _runGlobalSuperStep() -> futures::Future<Result>;
  auto _transformSendCountFromShardToServer(
      std::unordered_map<ShardID, uint64_t> sendCountPerShard) const
      -> std::unordered_map<ServerID, uint64_t>;
};

}  // namespace conductor
}  // namespace arangodb::pregel
