////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
  // This resign method is the one that will set the stateHandle to null.
  // It is only called from one place, where we hold a lock und which we also
  // deletes the owning struct.
  // Hence we cannot have a double resign method.
  TRI_ASSERT(guard->stateHandle != nullptr);
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
  // The stateHandle is initialized as non-null, it can be reset
  // to null via resign. As becomeFollower is part of the
  // single threaded setup process, no one could have called
  // resign.
  TRI_ASSERT(guard->stateHandle != nullptr)
      << "We did hand out references to StateHandleManager before completing "
         "the setup";
  guard->stateHandle->becomeFollower(std::move(ptr));
}

void StateHandleManager::acquireSnapshot(const ParticipantId& leader,
                                         std::uint64_t version) noexcept {
  if (auto guard = guardedData.getLockedGuard(); guard->stateHandle) {
    // TODO is the correct log index required here?
    guard->stateHandle->acquireSnapshot(leader, LogIndex{0}, version);
  }
}

auto StateHandleManager::getInternalStatus() const -> replicated_state::Status {
  auto guard = guardedData.getLockedGuard();
  if (guard->stateHandle == nullptr) {
    // We can only have a nullptr here if we have resigned,
    // also this stateHandleManager is only used in the follower
    // So we can directly return FollowerResigned here.
    return {replicated_state::Status::Follower{
        replicated_state::Status::Follower::Resigned{}}};
  }
  return guard->stateHandle->getInternalStatus();
}
