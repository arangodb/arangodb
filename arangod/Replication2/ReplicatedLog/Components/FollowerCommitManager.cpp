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

using namespace arangodb::replication2::replicated_log::comp;

auto FollowerCommitManager::updateCommitIndex(LogIndex index) noexcept
    -> DeferredAction {
  struct ResolveContext {
    WaitForResult result;
    InMemoryLog log;
    WaitForQueue queue;
  };

  auto ctx = std::make_unique<ResolveContext>();
  auto guard = guardedData.getLockedGuard();
  ctx->log = guard->storage.getCommittedLog();

  LOG_CTX("d2083", TRACE, loggerContext)
      << "received update commit index to " << index
      << " old commit index = " << guard->commitIndex
      << " old resolve index = " << guard->resolveIndex;

  guard->commitIndex = std::max(guard->commitIndex, index);
  auto newResolveIndex = std::min(guard->commitIndex, ctx->log.getLastIndex());

  if (newResolveIndex > guard->resolveIndex) {
    LOG_CTX("71a8f", DEBUG, loggerContext)
        << "resolving commit index up to " << newResolveIndex;
    guard->resolveIndex = newResolveIndex;
    guard->stateHandle.updateCommitIndex(guard->resolveIndex);
  }

  ctx->result = WaitForResult(guard->commitIndex, nullptr);

  auto const end = guard->waitQueue.upper_bound(guard->resolveIndex);
  for (auto it = guard->waitQueue.begin(); it != end;) {
    ctx->queue.insert(guard->waitQueue.extract(it++));
  }

  return DeferredAction([ctx = std::move(ctx)]() mutable noexcept {
    for (auto& it : ctx->queue) {
      if (!it.second.isFulfilled()) {
        // This only throws if promise was fulfilled earlier.
        it.second.setValue(std::make_pair(ctx->result, ctx->log));
      }
    }
  });
}

auto FollowerCommitManager::getCommitIndex() const noexcept -> LogIndex {
  return guardedData.getLockedGuard()->commitIndex;
}

FollowerCommitManager::FollowerCommitManager(IStorageManager& storage,
                                             IStateHandleManager& stateHandle,
                                             LoggerContext const& loggerContext)
    : guardedData(storage, stateHandle),
      loggerContext(loggerContext.with<logContextKeyLogComponent>(
          "follower-commit-manager")) {}

auto FollowerCommitManager::waitFor(LogIndex index) noexcept
    -> ILogParticipant::WaitForFuture {
  return waitForBoth(index).thenValue([](auto&& pair) { return pair.first; });
}

auto FollowerCommitManager::waitForIterator(LogIndex index) noexcept
    -> ILogParticipant::WaitForIteratorFuture {
  return waitForBoth(index).thenValue(
      [index](auto&& pair) -> std::unique_ptr<LogRangeIterator> {
        auto commitIndex = pair.first.currentCommitIndex;
        auto& log = pair.second;
        return log.getIteratorRange(index, commitIndex + 1);
      });
}

auto FollowerCommitManager::waitForBoth(LogIndex index) noexcept
    -> futures::Future<std::pair<WaitForResult, InMemoryLog>> {
  auto guard = guardedData.getLockedGuard();

  if (index <= guard->resolveIndex) {
    // resolve immediately
    auto result = WaitForResult(guard->commitIndex, nullptr);
    auto log = guard->storage.getCommittedLog();
    return std::make_pair(result, log);
  }

  return guard->waitQueue.emplace(index, ResolvePromise{})->second.getFuture();
}

FollowerCommitManager::GuardedData::GuardedData(
    IStorageManager& storage, IStateHandleManager& stateHandle)
    : storage(storage), stateHandle(stateHandle) {}
