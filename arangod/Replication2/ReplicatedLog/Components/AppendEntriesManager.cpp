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
#include "AppendEntriesManager.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedLog/Components/SnapshotManager.h"
#include "Replication2/coro-helper.h"
#include "Replication2/ReplicatedLog/Components/ICompactionManager.h"
#include "Replication2/ReplicatedLog/Components/IFollowerCommitManager.h"
#include "Replication2/DeferredExecution.h"

using namespace arangodb::replication2::replicated_log::comp;

auto AppendEntriesManager::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  Guarded<GuardedData>::mutex_guard_type guard = guarded.getLockedGuard();
  auto self = shared_from_this();  // required for coroutine to keep this alive
  auto requestGuard = guard->requestInFlight.acquire();
  if (not requestGuard) {
    co_return AppendEntriesResult::withRejection(
        LogTerm{1}, MessageId{0},
        AppendEntriesErrorReason{
            AppendEntriesErrorReason::ErrorType::kPrevAppendEntriesInFlight},
        false);
  }

  // TODO handle message ids properly
  // TODO add snapshot status to response
  // TODO metrics
  // TODO logging

  if (auto error = guard->preflightChecks(); error) {
    co_return {std::move(*error)};
  }

  {
    // Invalidate snapshot status if necessary.
    if (request.prevLogEntry == TermIndexPair{} &&
        request.entries.front().entry().logIndex() > LogIndex{1}) {
      // LOG_CTX("6262d", INFO, _loggerContext)
      //     << "Log truncated - invalidating snapshot";
      // triggers new snapshot transfer
      guard->snapshot.invalidateSnapshotState();
    }
  }

  {
    auto store = guard->storage.transaction();
    if (store->getInMemoryLog().getLastIndex() != request.prevLogEntry.index) {
      auto f = store->removeBack(request.prevLogEntry.index + 1);
      guard.unlock();
      auto result = co_await asResult(std::move(f));
      if (result.fail()) {
        co_return AppendEntriesResult::withPersistenceError(
            LogTerm{1}, MessageId{0}, result, false);
      }
      guard = self->guarded.getLockedGuard();
      store = guard->storage.transaction();
    }

    if (not request.entries.empty()) {
      auto f = store->appendEntries(InMemoryLog{request.entries});
      guard.unlock();
      auto result = co_await asResult(std::move(f));
      if (result.fail()) {
        co_return AppendEntriesResult::withPersistenceError(
            LogTerm{1}, MessageId{0}, result, false);
      }
      guard = self->guarded.getLockedGuard();
    }
  }

  guard->compaction.updateLargestIndexToKeep(request.lowestIndexToKeep);
  auto action = guard->commit.updateCommitIndex(request.leaderCommit);
  guard.unlock();
  action.fire();
  co_return AppendEntriesResult::withOk(LogTerm{1}, MessageId{0}, false);
}

AppendEntriesManager::AppendEntriesManager(IStorageManager& storage,
                                           ISnapshotManager& snapshot,
                                           ICompactionManager& compaction,
                                           IFollowerCommitManager& commit)
    : guarded(storage, snapshot, compaction, commit) {}

AppendEntriesManager::GuardedData::GuardedData(IStorageManager& storage,
                                               ISnapshotManager& snapshot,
                                               ICompactionManager& compaction,
                                               IFollowerCommitManager& commit)
    : storage(storage),
      snapshot(snapshot),
      compaction(compaction),
      commit(commit) {}

auto AppendEntriesManager::GuardedData::preflightChecks()
    -> std::optional<AppendEntriesResult> {
  std::abort();  // TODO
  return std::nullopt;
}
