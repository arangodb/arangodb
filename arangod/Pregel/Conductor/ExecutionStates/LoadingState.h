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

namespace arangodb::pregel::conductor {

struct ConductorState;

struct Loading : ExecutionState {
  Loading(ConductorState& conductor,
          std::unordered_map<ShardID, actor::ActorPID> actorForShard);
  ~Loading();
  auto name() const -> std::string override { return "loading"; };
  auto messages()
      -> std::unordered_map<actor::ActorPID,
                            worker::message::WorkerMessages> override;
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;

  ConductorState& conductor;
  std::unordered_map<ShardID, actor::ActorPID> actorForShard;
  std::unordered_set<actor::ActorPID> respondedWorkers;
  uint64_t totalVerticesCount = 0;
  uint64_t totalEdgesCount = 0;
};

}  // namespace arangodb::pregel::conductor
