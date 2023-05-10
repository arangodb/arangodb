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
#include "FollowerCommitManager.h"
#include "Replication2/DeferredExecution.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/IScheduler.h"

using namespace arangodb::replication2::replicated_log::comp;

auto FollowerCommitManager::updateCommitIndex(LogIndex index) noexcept
    -> std::pair<std::optional<LogIndex>, DeferredAction> {
  auto guard = guardedData.getLockedGuard();

  LOG_CTX("d2083", TRACE, loggerContext)
      << "received update commit index to " << index
      << " old commit index = " << guard->commitIndex
      << " old resolve index = " << guard->resolveIndex;

  auto localSpearhead =
      guard->storage.getTermIndexMapping().getLastIndex().value_or(
          TermIndexPair{});
  guard->commitIndex = std::max(guard->commitIndex, index);
  auto newResolveIndex = std::min(guard->commitIndex, localSpearhead.index);

  auto resolveIndex = std::optional<LogIndex>();
  if (newResolveIndex > guard->resolveIndex) {
    LOG_CTX("71a8f", TRACE, loggerContext)
        << "resolving commit index up to " << newResolveIndex;
    guard->resolveIndex = newResolveIndex;
    resolveIndex = newResolveIndex;
  }

  struct ResolveContext {
    WaitForResult result;
    WaitForQueue queue;
    std::shared_ptr<IScheduler> scheduler;
  };

  auto ctx = std::make_unique<ResolveContext>();
  ctx->scheduler = scheduler;
  ctx->result = WaitForResult(guard->commitIndex, nullptr);

  auto const end = guard->waitQueue.upper_bound(guard->resolveIndex);
  for (auto it = guard->waitQueue.begin(); it != end;) {
    ctx->queue.insert(guard->waitQueue.extract(it++));
  }

  return std::pair(
      resolveIndex, DeferredAction([ctx = std::move(ctx)]() mutable noexcept {
        for (auto& it : ctx->queue) {
          if (!it.second.isFulfilled()) {
            ctx->scheduler->queue([p = std::move(it.second),
                                   res = std::move(ctx->result)]() mutable {
              p.setValue(std::move(res));
            });
          }
        }
      }));
}

auto FollowerCommitManager::getCommitIndex() const noexcept -> LogIndex {
  return guardedData.getLockedGuard()->commitIndex;
}

FollowerCommitManager::FollowerCommitManager(
    IStorageManager& storage, LoggerContext const& loggerContext,
    std::shared_ptr<IScheduler> scheduler)
    : guardedData(storage),
      loggerContext(loggerContext.with<logContextKeyLogComponent>(
          "follower-commit-manager")),
      scheduler(std::move(scheduler)) {}

auto FollowerCommitManager::waitFor(LogIndex index) noexcept
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
    return WaitForResult(guard->commitIndex, nullptr);
  }

  return guard->waitQueue.emplace(index, ResolvePromise{})->second.getFuture();
}

auto FollowerCommitManager::waitForIterator(LogIndex index) noexcept
    -> ILogParticipant::WaitForIteratorFuture {
  return waitFor(index).thenValue(
      [index,
       weak = weak_from_this()](auto&&) -> std::unique_ptr<LogRangeIterator> {
        if (auto self = weak.lock(); self) {
          auto guard = self->guardedData.getLockedGuard();
          return guard->storage.getCommittedLogIterator(
              LogRange{index, guard->resolveIndex + 1});
        }

        THROW_ARANGO_EXCEPTION(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
      });
}

void FollowerCommitManager::resign() noexcept {
  auto guard = guardedData.getLockedGuard();
  ADB_PROD_ASSERT(not guard->isResigned);
  guard->isResigned = true;
  for (auto& [idx, promise] : guard->waitQueue) {
    promise.setException(ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE));
  }
  guard->waitQueue.clear();
}

FollowerCommitManager::GuardedData::GuardedData(IStorageManager& storage)
    : storage(storage) {}
