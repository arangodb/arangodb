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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "StateHandleManager.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/Components/IFollowerCommitManager.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::comp;

StateHandleManager::GuardedData::GuardedData(
    std::unique_ptr<IReplicatedStateHandle> stateHandlePtr,
    IFollowerCommitManager& commit)
    : stateHandle(std::move(stateHandlePtr)), commit(commit) {
  ADB_PROD_ASSERT(stateHandle != nullptr);
}

auto StateHandleManager::resign() noexcept
    -> std::unique_ptr<IReplicatedStateHandle> {
  auto guard = guardedData.getLockedGuard();
  // TODO assert same methods
  std::ignore = guard->stateHandle->resignCurrentState();
  return std::move(guard->stateHandle);
}

auto StateHandleManager::updateCommitIndex(LogIndex index,
                                           bool snapshotAvailable) noexcept
    -> DeferredAction {
  if (auto guard = guardedData.getLockedGuard(); guard->stateHandle) {
    auto&& [maybeResolveIndex, action] =
        guard->commit.updateCommitIndex(index, snapshotAvailable);
    if (maybeResolveIndex) {
      guard->stateHandle->updateCommitIndex(*maybeResolveIndex);
    }
    return std::move(action);
  } else {
    // TODO Do some logging here
    return {};
  }
}

StateHandleManager::StateHandleManager(
    std::unique_ptr<IReplicatedStateHandle> stateHandle,
    IFollowerCommitManager& commit)
    : guardedData(std::move(stateHandle), commit) {}

void StateHandleManager::becomeFollower(
    std::unique_ptr<IReplicatedLogFollowerMethods> ptr) {
  auto guard = guardedData.getLockedGuard();
  guard->stateHandle->becomeFollower(std::move(ptr));
}

void StateHandleManager::acquireSnapshot(const ParticipantId& leader,
                                         std::uint64_t version) noexcept {
  auto guard = guardedData.getLockedGuard();
  // TODO is the correct log index required here?
  guard->stateHandle->acquireSnapshot(leader, LogIndex{0}, version);
}

auto StateHandleManager::getInternalStatus() const -> replicated_state::Status {
  return guardedData.getLockedGuard()->stateHandle->getInternalStatus();
}
