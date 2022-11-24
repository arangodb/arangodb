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
#pragma once

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/WaitForBag.h"
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"

#include <Basics/Guarded.h>
#include <Futures/Future.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>

#include "Replication2/ReplicatedLog/Components/SnapshotManager.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedLog/Components/CompactionManager.h"
#include "Replication2/ReplicatedLog/Components/FollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/AppendEntriesManager.h"

namespace arangodb::replication2::replicated_log::refactor {
/*
struct IStateHandleManager {
  virtual ~IStateHandleManager() = default;
  virtual void updateCommitIndex(LogIndex) noexcept = 0;
  virtual void resign() noexcept = 0;
};

struct StateHandleManager : IStateHandleManager {
  void resign() noexcept override {
    // TODO assert same methods
    std::ignore = stateHandle->resignCurrentState();
  }

  void updateCommitIndex(LogIndex index) noexcept override {
    stateHandle->updateCommitIndex(index);
  }

 private:
  std::shared_ptr<IReplicatedStateHandle> const stateHandle;
};
*/

struct FollowerManager {
  explicit FollowerManager(
      std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
      std::shared_ptr<ReplicatedLogGlobalSettings const> options)
      : storage(std::make_shared<StorageManager>(std::move(methods))),
        compaction(std::make_shared<CompactionManager>(*storage, options)),
        snapshot(std::make_shared<SnapshotManager>(*storage)),
        commit(std::make_shared<FollowerCommitManager>(*storage)),
        appendEntries(std::make_shared<AppendEntriesManager>(
            *storage, *snapshot, *compaction, *commit)) {}

  auto getStatus() const -> LogStatus {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto getQuickStatus() const -> QuickLogStatus {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto resign() -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  std::shared_ptr<StorageManager> const storage;
  std::shared_ptr<CompactionManager> const compaction;
  std::shared_ptr<SnapshotManager> const snapshot;
  std::shared_ptr<FollowerCommitManager> const commit;
  std::shared_ptr<AppendEntriesManager> const appendEntries;
};

struct MethodsProvider : IReplicatedLogFollowerMethods {
  auto releaseIndex(LogIndex index) -> void override {
    follower.compaction->updateReleaseIndex(index);
  }

  auto getLogSnapshot() -> InMemoryLog override {
    return follower.storage->getCommittedLog();
  }

  auto waitFor(LogIndex index) -> ILogParticipant::WaitForFuture override {
    return follower.commit->waitFor(index);
  }

  auto waitForIterator(LogIndex index)
      -> ILogParticipant::WaitForIteratorFuture override {
    return follower.commit->waitForIterator(index);
  }

  auto snapshotCompleted() -> Result override {
    return follower.snapshot->updateSnapshotState(SnapshotState::AVAILABLE);
  }

 private:
  FollowerManager& follower;
};

struct LogFollowerImpl : ILogFollower {
  auto getStatus() const -> LogStatus override {
    return guarded.getLockedGuard()->getStatus();
  }

  auto getQuickStatus() const -> QuickLogStatus override {
    return guarded.getLockedGuard()->getQuickStatus();
  }

  auto
  resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> override {
    return guarded.getLockedGuard()->resign();
  }

  auto waitFor(LogIndex index) -> WaitForFuture override {
    return guarded.getLockedGuard()->commit->waitFor(index);
  }
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
    return guarded.getLockedGuard()->commit->waitForIterator(index);
  }

  auto copyInMemoryLog() const -> InMemoryLog override {
    return guarded.getLockedGuard()->storage->getCommittedLog();
  }

  auto release(LogIndex doneWithIdx) -> Result override {
    guarded.getLockedGuard()->compaction->updateReleaseIndex(doneWithIdx);
    return {};
  }

  auto compact() -> ResultT<CompactionResult> override {
    // TODO clean up CompacionResult vs ICompactionManager::CompactResult
    auto result = guarded.getLockedGuard()->compaction->compact().get();
    if (result.error) {
      return Result{result.error->errorNumber(), result.error->errorMessage()};
    }
    return CompactionResult{
        .numEntriesCompacted = result.compactedRange.count(),
        .range = result.compactedRange,
        .stopReason = result.stopReason};
  }

  auto getParticipantId() const noexcept -> ParticipantId const& override {
    return myself;
  }

  auto appendEntries(AppendEntriesRequest request)
      -> futures::Future<AppendEntriesResult> override {
    return guarded.getLockedGuard()->appendEntries->appendEntries(
        std::move(request));
  }

  ParticipantId const myself;

  Guarded<FollowerManager> guarded;
};

}  // namespace arangodb::replication2::replicated_log::refactor
