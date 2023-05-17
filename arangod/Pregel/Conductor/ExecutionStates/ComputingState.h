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

class MasterContext;

namespace conductor {

struct ConductorState;

struct Computing : ExecutionState {
  Computing(ConductorState& conductor,
            std::unique_ptr<MasterContext> masterContext,
            std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor,
            uint64_t totalSendMessagesCount,
            uint64_t totalReceivedMessagesCount);
  ~Computing() override = default;
  auto name() const -> std::string override { return "computing"; };
  auto messages()
      -> std::unordered_map<actor::ActorPID,
                            worker::message::WorkerMessages> override;
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;
  auto cancel(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<StateChange> override;
  struct PostGlobalSuperStepResult {
    bool finished;
  };
  auto _postGlobalSuperStep() -> PostGlobalSuperStepResult;
  auto _aggregateMessage(message::GlobalSuperStepFinished msg) -> void;

  ConductorState& conductor;
  std::unique_ptr<MasterContext> masterContext;
  std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor;

  std::unordered_set<actor::ActorPID> respondedWorkers;
  uint64_t totalSendMessagesCount;
  uint64_t totalReceivedMessagesCount;
  uint64_t activeCount = 0;
  uint64_t vertexCount = 0;
  uint64_t edgeCount = 0;
  VPackBuilder aggregators;
  std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActorForNextGss;
};

}  // namespace conductor
}  // namespace arangodb::pregel
