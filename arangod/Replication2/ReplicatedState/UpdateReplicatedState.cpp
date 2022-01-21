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
#include "Logger/LogMacros.h"
#include "UpdateReplicatedState.h"
#include "Replication2/ReplicatedState/StateCommon.h"
#include "Basics/voc-errors.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

auto algorithms::updateReplicatedState(
    StateActionContext& ctx, std::string const& serverId, LogId id,
    replicated_state::agency::Plan const* spec) -> arangodb::Result {
  if (spec == nullptr) {
    return ctx.dropReplicatedState(id);
  }

  TRI_ASSERT(id == spec->id);

  auto state = ctx.getReplicatedStateById(id);
  if (state == nullptr) {
    // TODO use user data instead of non-slice
    // here we have to put in the Current/Snapshot data here as well.
    auto result =
        ctx.createReplicatedState(id, spec->properties.implementation.type,
                                  velocypack::Slice::noneSlice());
    if (result.fail()) {
      return result.result();
    }

    state = result.get();
    state->flush(std::make_unique<ReplicatedStateCore>());
    return {TRI_ERROR_NO_ERROR};
  } else {
    auto status = state->getSnapshotStatus();
    TRI_ASSERT(spec->participants.contains(serverId));
    if (status.generation != spec->participants.at(serverId).generation) {
      state->flush(nullptr);
    }
    return {TRI_ERROR_NO_ERROR};
  }
}
