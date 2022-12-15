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
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Basics/application-exit.h"
#include "Logger/LogContextKeys.h"

using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::comp;

auto SnapshotManager::checkSnapshotState() noexcept -> SnapshotState {
  return guardedData.getLockedGuard()->state;
}

auto SnapshotManager::invalidateSnapshotState() -> arangodb::Result {
  return updateSnapshotState(SnapshotState::MISSING);
}

auto SnapshotManager::updateSnapshotState(SnapshotState state) -> Result {
  if (auto guard = guardedData.getLockedGuard(); guard->state != state) {
    auto trx = guard->storage.beginMetaInfoTrx();
    trx->get().snapshot.status =
        state == SnapshotState::AVAILABLE
            ? replicated_state::SnapshotStatus::kCompleted
            : replicated_state::SnapshotStatus::kInvalidated;
    auto result = guard->storage.commitMetaInfoTrx(std::move(trx));
    if (result.fail()) {
      return result;
    }

    LOG_CTX("b2c65", INFO, loggerContext)
        << "snapshot status locally updated to " << to_string(state);
    guard->state = state;
    ADB_PROD_ASSERT(termInfo->leader.has_value());
    auto& stateHandle = guard->stateHandle;
    guard.unlock();
    if (state == SnapshotState::MISSING) {
      stateHandle.acquireSnapshot(*termInfo->leader);
    }
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
    std::shared_ptr<ILeaderCommunicator> leaderComm,
    LoggerContext const& loggerContext)
    : leaderComm(std::move(leaderComm)),
      termInfo(std::move(termInfo)),
      loggerContext(
          loggerContext.with<logContextKeyLogComponent>("snapshot-man")),
      guardedData(storage, stateHandle) {}

auto SnapshotManager::setSnapshotStateAvailable(
    arangodb::replication2::replicated_log::MessageId msgId)
    -> arangodb::Result {
  auto result = updateSnapshotState(SnapshotState::AVAILABLE);
  leaderComm->reportSnapshotAvailable(msgId).thenFinal(
      [lctx = loggerContext](auto&& tryResult) {
        auto result = basics::catchToResult([&] { return tryResult.get(); });
        if (result.fail()) {
          LOG_CTX("eb674", FATAL, lctx) << "failed to snapshot state on leader";
          FATAL_ERROR_EXIT();
        }
        LOG_CTX("b2d65", INFO, lctx) << "snapshot status updated on leader";
      });
  return result;
}

auto comp::to_string(SnapshotState state) noexcept -> std::string_view {
  switch (state) {
    case SnapshotState::MISSING:
      return "MISSING";
    case SnapshotState::AVAILABLE:
      return "AVAILABLE";
  }
  std::abort();
}
