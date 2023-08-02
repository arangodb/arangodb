////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <functional>
#include "Pregel/Worker/Messages.h"
#include "Pregel/StatusMessages.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/ResultMessages.h"
#include "Pregel/SpawnMessages.h"

namespace arangodb::pregel::worker {

typedef std::function<void(pregel::message::StatusMessages)> DispatchStatus;
typedef std::function<void(pregel::metrics::message::MetricsMessages)>
    DispatchMetrics;
typedef std::function<void(pregel::conductor::message::ConductorMessages)>
    DispatchConductor;
typedef std::function<void(message::WorkerMessages)> DispatchSelf;
typedef std::function<void(actor::ActorPID, message::WorkerMessages)>
    DispatchOther;
typedef std::function<void(pregel::message::ResultMessages)> DispatchResult;
typedef std::function<void(pregel::message::SpawnMessages)> DispatchSpawn;

struct Dispatcher {
  DispatchStatus const& dispatchStatus;
  DispatchMetrics const& dispatchMetrics;
  DispatchConductor const& dispatchConductor;
  DispatchSelf const& dispatchSelf;
  DispatchOther const& dispatchOther;
  DispatchResult const& dispatchResult;
  DispatchSpawn const& dispatchSpawn;
};

struct ExecutionState {
  virtual ~ExecutionState() = default;

  [[nodiscard]] virtual auto name() const -> std::string = 0;
  virtual auto receive(actor::ActorPID const& sender,
                       actor::ActorPID const& self,
                       worker::message::WorkerMessages const& message,
                       Dispatcher dispatcher)
      -> std::unique_ptr<ExecutionState> {
    return nullptr;
  };
};
}  // namespace arangodb::pregel::worker
