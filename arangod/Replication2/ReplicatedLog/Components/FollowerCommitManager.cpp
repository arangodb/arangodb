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
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "WaitQueueManager.h"

using namespace arangodb::replication2::replicated_log::comp;

auto FollowerCommitManager::updateCommitIndex(LogIndex index) noexcept
    -> DeferredAction {
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

  if (newResolveIndex > guard->resolveIndex) {
    LOG_CTX("71a8f", TRACE, loggerContext)
        << "resolving commit index up to " << newResolveIndex;
    guard->resolveIndex = newResolveIndex;
    guard->stateHandle.updateCommitIndex(guard->resolveIndex);
  }

  auto result = WaitForResult(guard->commitIndex, nullptr);
  return waitQueue->resolveIndex(newResolveIndex, result);
}

auto FollowerCommitManager::getCommitIndex() const noexcept -> LogIndex {
  return guardedData.getLockedGuard()->commitIndex;
}

FollowerCommitManager::FollowerCommitManager(
    IStorageManager& storage, IStateHandleManager& stateHandle,
    std::shared_ptr<IWaitQueueManager> waitQueue,
    LoggerContext const& loggerContext)
    : guardedData(storage, stateHandle),
      loggerContext(loggerContext.with<logContextKeyLogComponent>(
          "follower-commit-manager")),
      waitQueue(std::move(waitQueue)) {}

FollowerCommitManager::GuardedData::GuardedData(
    IStorageManager& storage, IStateHandleManager& stateHandle)
    : storage(storage), stateHandle(stateHandle) {}
