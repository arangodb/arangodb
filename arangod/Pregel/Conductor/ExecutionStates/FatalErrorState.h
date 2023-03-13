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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <unordered_map>
#include "Pregel/Conductor/Messages.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/Worker/Messages.h"
#include "State.h"

namespace arangodb::pregel::conductor {

struct ConductorState;

struct FatalError : ExecutionState {
  FatalError(ConductorState& conductor) : conductor{conductor} {
    conductor.timing.total.finish();
  }
  ~FatalError() {}
  auto name() const -> std::string override { return "fatal error"; };
  auto message() -> worker::message::WorkerMessages override {
    return worker::message::WorkerMessages{};
  };
  auto receive(actor::ActorPID sender, message::ConductorMessages message)
      -> std::optional<std::unique_ptr<ExecutionState>> override {
    return std::nullopt;
  };
  ConductorState& conductor;
};

}  // namespace arangodb::pregel::conductor
