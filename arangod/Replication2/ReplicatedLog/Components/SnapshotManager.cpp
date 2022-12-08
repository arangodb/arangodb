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

#include "SnapshotManager.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedLog/Components/TermInformation.h"

using namespace arangodb::replication2::replicated_log::comp;

auto SnapshotManager::checkSnapshotState() noexcept -> SnapshotState {
  return guardedData.getLockedGuard()->state;
}

auto SnapshotManager::invalidateSnapshotState() -> arangodb::Result {
  return updateSnapshotState(SnapshotState::MISSING);
}

auto SnapshotManager::updateSnapshotState(SnapshotState state) -> Result {
  auto guard = guardedData.getLockedGuard();
  if (guard->state != state) {
    auto trx = guard->storage.beginMetaInfoTrx();
    trx->get().snapshot.status =
        state == SnapshotState::AVAILABLE
            ? replicated_state::SnapshotStatus::kCompleted
            : replicated_state::SnapshotStatus::kInvalidated;
    auto result = guard->storage.commitMetaInfoTrx(std::move(trx));
    if (result.fail()) {
      return result;
    }
    guard->state = state;
    ADB_PROD_ASSERT(termInfo->leader.has_value());
    guard->stateHandle.acquireSnapshot(*termInfo->leader);
  }
  return {};
}

SnapshotManager::GuardedData::GuardedData(IStorageManager& storage,
                                          IStateHandleManager& stateHandle)
    : storage(storage), stateHandle(stateHandle) {
  auto meta = storage.getCommittedMetaInfo();
  state = meta.snapshot.status == replicated_state::SnapshotStatus::kCompleted
              ? SnapshotState::AVAILABLE
              : SnapshotState::MISSING;
}

SnapshotManager::SnapshotManager(
    IStorageManager& storage, IStateHandleManager& stateHandle,
    std::shared_ptr<FollowerTermInformation const> termInfo,
    std::shared_ptr<ILeaderCommunicator> leaderComm)
    : leaderComm(std::move(leaderComm)),
      termInfo(std::move(termInfo)),
      guardedData(storage, stateHandle) {}
