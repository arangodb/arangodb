////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

namespace arangodb::replication2::algorithms {

struct StateActionContext {
  virtual ~StateActionContext() = default;

  virtual auto getReplicatedStateById(LogId) noexcept
      -> std::shared_ptr<replicated_state::ReplicatedStateBase> = 0;

  virtual auto createReplicatedState(LogId, std::string_view, velocypack::Slice)
      -> ResultT<std::shared_ptr<replicated_state::ReplicatedStateBase>> = 0;

  virtual auto dropReplicatedState(LogId) -> Result = 0;
};

auto updateReplicatedState(StateActionContext& ctx, std::string const& serverId,
                           LogId id, replicated_state::agency::Plan const* spec,
                           replicated_state::agency::Current const* current)
    -> Result;
}  // namespace arangodb::replication2::algorithms
