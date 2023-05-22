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

#include "State.h"

namespace arangodb::pregel::worker {
template<typename V, typename E, typename M>
struct WorkerState;

template<typename V, typename E, typename M>
struct Storing : ExecutionState {
  explicit Storing(WorkerState<V, E, M>& worker);
  ~Storing() override = default;

  [[nodiscard]] auto name() const -> std::string override { return "storing"; };
  auto receive(actor::ActorPID const& sender, actor::ActorPID const& self,
               message::WorkerMessages const& message, Dispatcher dispatcher)
      -> std::unique_ptr<ExecutionState> override;

  WorkerState<V, E, M>& worker;
};
}  // namespace arangodb::pregel::worker
