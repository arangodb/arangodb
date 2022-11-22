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
#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedLog/Components/SnapshotManager.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"

using namespace arangodb::replication2::replicated_log::comp;

auto AppendEntriesManager::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  auto guard = guarded.getLockedGuard();
  auto requestGuard = guard->requestInFlight.acquire();
  if (not requestGuard) {
    return AppendEntriesResult::withRejection(
        LogTerm{1}, MessageId{0},
        AppendEntriesErrorReason{
            AppendEntriesErrorReason::ErrorType::kPrevAppendEntriesInFlight},
        false);
  }

  if (auto error = guard->preflightChecks(); error) {
    return {std::move(*error)};
  }

  {
    // Invalidate snapshot status if necessary.
    if (request.prevLogEntry == TermIndexPair{} &&
        request.entries.front().entry().logIndex() > LogIndex{1}) {
      // LOG_CTX("6262d", INFO, _loggerContext)
      //     << "Log truncated - invalidating snapshot";
      // triggers new snapshot transfer
      guard->snapshot.updateSnapshotState(SnapshotState::MISSING);
    }
  }

  auto f = std::invoke(
      [&](Guarded<GuardedData>::mutex_guard_type&&) -> futures::Future<Result> {
        auto store = guard->storage.transaction();
        if (store->getInMemoryLog().getLastIndex() !=
            request.prevLogEntry.index) {
          return store->removeBack(request.prevLogEntry.index + 1);
        } else {
          return {TRI_ERROR_NO_ERROR};
        }
      },
      std::move(guard));

  return std::move(f).thenValue(
      [request = std::move(request),
       self = shared_from_this()](Result const& result) mutable
      -> futures::Future<AppendEntriesResult> {
        if (result.fail()) {
          return AppendEntriesResult::withPersistenceError(
              LogTerm{1}, MessageId{0}, result, false);
        }

        auto guard = self->guarded.getLockedGuard();
        auto store = guard->storage.transaction();
        auto f = store->appendEntries(InMemoryLog{request.entries});
        guard.unlock();
        return std::move(f).thenValue([self](Result const& result) {
          if (result.fail()) {
            return AppendEntriesResult::withPersistenceError(
                LogTerm{1}, MessageId{0}, result, false);
          }

          return AppendEntriesResult::withOk(LogTerm{1}, MessageId{0}, false);
        });
      });
}

AppendEntriesManager::AppendEntriesManager(IStorageManager& storage,
                                           ISnapshotManager& snapshot)
    : guarded(storage, snapshot) {}

AppendEntriesManager::GuardedData::GuardedData(IStorageManager& storage,
                                               ISnapshotManager& snapshot)
    : storage(storage), snapshot(snapshot) {}

auto AppendEntriesManager::GuardedData::preflightChecks()
    -> std::optional<AppendEntriesResult> {
  return std::nullopt;
}
