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

#include <set>
#include "Actor/ActorPID.h"
#include "State.h"

namespace arangodb::pregel::conductor {

struct ConductorState;

/*
   This state identifies the servers that are relevant for the given graph (via
   the involved shards) and creates workers on these servers.

   This state differes from the other states in two aspects:
   1. The receiving workers are created during this state, therefore the
   ActorPIDs of the workers are not known when this state starts running.
   2. Each relevant server receives a different message.
 */

struct CreateWorkers : ExecutionState {
  CreateWorkers(ConductorState& conductor);
  ~CreateWorkers() = default;
  auto name() const -> std::string override { return "create workers"; };
  /*
    Due to the mentioned specialities of this state, it has a special messages
    function that needs to be used instead of the message function of the state
    interface.
   */
  auto messagesToServers()
      -> std::unordered_map<ServerID, worker::message::CreateWorker>;
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;
  auto cancel(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;

  ConductorState& conductor;
  std::unordered_map<ShardID, actor::ActorPID> actorForShard;
  std::set<ServerID> sentServers;
  std::set<ServerID> respondedServers;
  uint64_t responseCount = 0;

 private:
  auto _updateResponsibleActorPerShard(actor::ActorPID actor) -> void;
};

}  // namespace arangodb::pregel::conductor
