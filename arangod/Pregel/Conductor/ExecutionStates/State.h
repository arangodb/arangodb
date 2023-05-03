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

#include <string>
#include "Actor/ActorPID.h"
#include "Cluster/ClusterTypes.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/StatusMessages.h"
#include "Pregel/MetricsMessages.h"
#include "Pregel/Worker/Messages.h"

namespace arangodb::pregel::conductor {

struct ExecutionState;

struct StateChange {
  std::optional<pregel::message::StatusMessages> statusMessage = std::nullopt;
  std::optional<pregel::metrics::message::MetricsMessages> metricsMessage =
      std::nullopt;
  std::unique_ptr<ExecutionState> newState = nullptr;
};

#define LOG_PREGEL_CONDUCTOR_STATE(logId, level)              \
  LOG_TOPIC(logId, level, Logger::PREGEL)                     \
      << "[job " << conductor._specifications.executionNumber \
      << "] Conductor " << name() << " state "

struct ExecutionState {
  virtual auto name() const -> std::string = 0;
  virtual auto messages()
      -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
    return {};
  }
  virtual auto receive(actor::ActorPID sender,
                       conductor::message::ConductorMessages message)
      -> std::optional<StateChange> {
    return std::nullopt;
  };
  virtual auto cancel(actor::ActorPID sender,
                      conductor::message::ConductorMessages message)
      -> std::optional<StateChange> {
    return std::nullopt;
  };
  virtual auto aqlResultsAvailable() const -> bool { return false; }
  virtual ~ExecutionState() = default;
};

}  // namespace arangodb::pregel::conductor
