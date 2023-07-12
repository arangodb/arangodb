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

#include "Basics/application-exit.h"
#include "Basics/Exceptions.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/Components/TermInformation.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/Storage/PersistedStateInfo.h"

using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::comp;

/*
 * on invalidate snapshot
 * 1. Persist on disk, that no snapshot is available.
 * 2. Call acquire snapshot on state handle, with new version.
 *
 * on setSnapshotStateAvailable:
 * 1. check correct version
 * 2. persist snapshot state
 * 3. update leader via LeaderComm
 *
 * on startup:
 * 1. if no snapshot available:
 *    1.1. if there is leader => acquireSnapshot
 */

auto SnapshotManager::invalidateSnapshotState() -> arangodb::Result {
  auto guard = guardedData.getLockedGuard();
  if (guard->state == SnapshotState::AVAILABLE) {
    auto result = guard->updatePersistedSnapshotState(SnapshotState::MISSING);
    if (result.fail()) {
      LOG_CTX("0601b", ERR, loggerContext)
          << "failed to persist information that snapshot is missing";
      return result;
    }
  }
  LOG_CTX("6b38e", INFO, loggerContext) << "invalidating snapshot";
  auto& stateHandle = guard->stateHandle;
  auto newVersion = ++guard->lastSnapshotVersion;
  guard.unlock();
  LOG_CTX("a5f6f", DEBUG, loggerContext)
      << "acquiring new snapshot with version " << newVersion;
  stateHandle.acquireSnapshot(*termInfo->leader, newVersion);
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

auto SnapshotManager::GuardedData::updatePersistedSnapshotState(
    SnapshotState newState) -> arangodb::Result {
  auto trx = storage.beginMetaInfoTrx();
  trx->get().snapshot.status =
      newState == SnapshotState::AVAILABLE
          ? replicated_state::SnapshotStatus::kCompleted
          : replicated_state::SnapshotStatus::kInvalidated;
  auto result = storage.commitMetaInfoTrx(std::move(trx));
  if (result.fail()) {
    return result;
  }
  state = newState;
  return {};
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

void SnapshotManager::acquireSnapshotIfNecessary() {
  auto guard = guardedData.getLockedGuard();
  if (termInfo->leader.has_value() and guard->state == SnapshotState::MISSING) {
    auto version = ++guard->lastSnapshotVersion;
    auto& stateHandle = guard->stateHandle;
    guard.unlock();
    LOG_CTX("5426a", INFO, loggerContext)
        << "detected missing snapshot - acquire new one";
    stateHandle.acquireSnapshot(*termInfo->leader, version);
  }
}

auto SnapshotManager::setSnapshotStateAvailable(MessageId msgId,
                                                std::uint64_t version)
    -> arangodb::Result {
  auto guard = guardedData.getLockedGuard();
  if (guard->lastSnapshotVersion != version) {
    LOG_CTX("eb008", INFO, loggerContext)
        << "dismiss snapshot available message - wrong version, found "
        << version << " expected " << guard->lastSnapshotVersion;
    return {};
  }

  {
    auto result = guard->updatePersistedSnapshotState(SnapshotState::AVAILABLE);
    if (result.fail()) {
      LOG_CTX("52cac", ERR, loggerContext)
          << "Failed to update snapshot information: " << result.errorMessage();
      return result;
    }
  }

  guard.unlock();

  leaderComm->reportSnapshotAvailable(msgId).thenFinal(
      [lctx = loggerContext](auto&& tryResult) {
        auto result = basics::catchToResult([&] { return tryResult.get(); });
        if (result.fail()) {
          LOG_CTX("eb674", FATAL, lctx) << "failed to snapshot state on leader";
          FATAL_ERROR_EXIT();
        }
        LOG_CTX("b2d65", INFO, lctx) << "snapshot status updated on leader";
      });
  return {};
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

auto SnapshotManager::checkSnapshotState() const noexcept -> SnapshotState {
  return guardedData.getLockedGuard()->state;
}
