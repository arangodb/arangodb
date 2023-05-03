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

namespace arangodb::pregel::worker {

typedef std::function<void(pregel::message::StatusMessages)> DispatchStatus;
typedef std::function<void(pregel::metrics::message::MetricsMessages)>
    DispatchMetrics;
typedef std::function<void(pregel::conductor::message::ConductorMessages)>
    DispatchConductor;
typedef std::function<void(message::WorkerMessages)> DispatchSelf;
typedef std::function<void(actor::ActorPID, message::WorkerMessages)>
    DispatchOther;

struct ExecutionState {
  virtual ~ExecutionState() = default;

  [[nodiscard]] virtual auto name() const -> std::string = 0;
  virtual auto receive(actor::ActorPID const& sender,
                       worker::message::WorkerMessages const& message,
                       DispatchStatus const& dispatchStatus,
                       DispatchMetrics const& dispatchMetrics,
                       DispatchConductor const& dispatchConductor,
                       DispatchSelf const& dispatchSelf,
                       DispatchOther const& dispatchOther)
      -> std::unique_ptr<ExecutionState> {
    return nullptr;
  };
  virtual auto cancel(actor::ActorPID const& sender,
                      worker::message::WorkerMessages const& message)
      -> std::unique_ptr<ExecutionState> {
    return nullptr;
  };
};
}  // namespace arangodb::pregel::worker
