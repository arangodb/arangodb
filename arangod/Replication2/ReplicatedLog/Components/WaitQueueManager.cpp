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

#include "WaitQueueManager.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "IStorageManager.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto WaitQueueManager::waitFor(LogIndex index) noexcept
    -> ILogParticipant::WaitForFuture {
  auto guard = guardedData.getLockedGuard();

  if (guard->isResigned) {
    auto promise = ResolvePromise{};
    promise.setException(ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE));
    return promise.getFuture();
  }

  if (index <= guard->resolveIndex) {
    // resolve immediately
    return WaitForResult(guard->resolveIndex, nullptr);
  }

  return guard->waitQueue.emplace(index, ResolvePromise{})->second.getFuture();
}

auto WaitQueueManager::waitForIterator(LogIndex index) noexcept
    -> ILogParticipant::WaitForIteratorFuture {
  return waitFor(index).thenValue(
      [index,
       weak = weak_from_this()](auto&&) -> std::unique_ptr<LogRangeIterator> {
        if (auto self = weak.lock(); self) {
          auto guard = self->guardedData.getLockedGuard();
          return self->storage->getCommittedLogIterator(
              LogRange{index, guard->resolveIndex + 1});
        }

        THROW_ARANGO_EXCEPTION(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
      });
}
void WaitQueueManager::resign() noexcept {
  auto guard = guardedData.getLockedGuard();
  ADB_PROD_ASSERT(not guard->isResigned);
  guard->isResigned = true;
  for (auto& [idx, promise] : guard->waitQueue) {
    promise.setException(ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE));
  }
  guard->waitQueue.clear();
}
WaitQueueManager::WaitQueueManager(std::shared_ptr<IStorageManager> storage)
    : storage(std::move(storage)) {}

auto WaitQueueManager::resolveIndex(LogIndex, WaitForResult) noexcept
    -> DeferredAction {
  auto guard = guardedData.getLockedGuard();
  struct ResolveContext {
    WaitForResult result;
    WaitForQueue queue;
  };

  auto ctx = std::make_unique<ResolveContext>();

  ctx->result = WaitForResult(guard->resolveIndex, nullptr);

  auto const end = guard->waitQueue.upper_bound(guard->resolveIndex);
  for (auto it = guard->waitQueue.begin(); it != end;) {
    ctx->queue.insert(guard->waitQueue.extract(it++));
  }

  return DeferredAction([ctx = std::move(ctx)]() mutable noexcept {
    for (auto& it : ctx->queue) {
      if (!it.second.isFulfilled()) {
        // This only throws if promise was fulfilled earlier.
        it.second.setValue(ctx->result);
      }
    }
  });
}
