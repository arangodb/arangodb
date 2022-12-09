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

#include "CompactionManager.h"
#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Logger/LogContextKeys.h"

using namespace arangodb::replication2::replicated_log::comp;

CompactionManager::CompactionManager(
    IStorageManager& storage,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    LoggerContext const& loggerContext)
    : guarded(storage),
      loggerContext(
          loggerContext.with<logContextKeyLogComponent>("compaction-man")),
      options(std::move(options)) {}

void CompactionManager::updateReleaseIndex(LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  LOG_CTX_IF("641f7", TRACE, loggerContext, index > guard->releaseIndex)
      << "updating release index for compaction to " << index;
  guard->releaseIndex = std::max(guard->releaseIndex, index);
  triggerAsyncCompaction(std::move(guard), false);
}

void CompactionManager::updateLargestIndexToKeep(LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  LOG_CTX_IF("ff33a", TRACE, loggerContext, index > guard->largestIndexToKeep)
      << "updating largest index to keep to " << index;
  guard->largestIndexToKeep = std::max(guard->largestIndexToKeep, index);
  triggerAsyncCompaction(std::move(guard), false);
}

auto CompactionManager::compact() noexcept -> futures::Future<CompactResult> {
  auto guard = guarded.getLockedGuard();
  auto f = guard->compactAggregator.waitFor();
  LOG_CTX("43337", INFO, loggerContext) << "triggering manual compaction";
  triggerAsyncCompaction(std::move(guard), true);
  return f;
}

auto CompactionManager::getCompactionStatus() const noexcept
    -> CompactionStatus {
  return guarded.getLockedGuard()->status;
}

CompactionManager::GuardedData::GuardedData(IStorageManager& storage)
    : storage(storage) {}

auto CompactionManager::GuardedData::isCompactionInProgress() const noexcept
    -> bool {
  return _compactionInProgress;
}

auto CompactionManager::calculateCompactionIndex(GuardedData const& data,
                                                 LogRange bounds,
                                                 std::size_t threshold)
    -> std::tuple<LogIndex, CompactionStopReason> {
  // TODO make this function a stand alone functional program
  //      and write tests for it, separately.
  auto [first, last] = bounds;
  auto newCompactionIndex =
      std::min(data.releaseIndex, data.largestIndexToKeep);
  auto nextAutomaticCompactionAt = first + threshold;
  if (nextAutomaticCompactionAt > newCompactionIndex) {
    return {first,
            {CompactionStopReason::CompactionThresholdNotReached{
                nextAutomaticCompactionAt}}};
  }

  if (first == last) {
    return {first, {CompactionStopReason::NothingToCompact{}}};
  } else if (newCompactionIndex == data.releaseIndex) {
    return {
        newCompactionIndex,
        {CompactionStopReason::NotReleasedByStateMachine{data.releaseIndex}}};
  } else {
    TRI_ASSERT(newCompactionIndex == data.largestIndexToKeep);
    return {newCompactionIndex,
            {CompactionStopReason::LeaderBlocksReleaseEntry{}}};
  }
}

void CompactionManager::checkCompaction(
    Guarded<GuardedData>::mutex_guard_type guard) {
  auto store = guard->storage.transaction();
  auto logBounds = store->getLogBounds();

  auto threshold =
      guard->_fullCompactionNextRound ? 0 : options->_thresholdLogCompaction;
  auto [index, reason] =
      calculateCompactionIndex(guard.get(), logBounds, threshold);
  guard->_fullCompactionNextRound = false;
  auto promises = std::move(guard->compactAggregator);

  // TODO maybe we want to rewrite this as a coroutine?
  if (index > logBounds.from) {
    auto& compaction = guard->status.inProgress.emplace();
    compaction.time = CompactionStatus::clock ::now();
    LogRange compactionRange = LogRange{logBounds.from, index};
    compaction.range = compactionRange;
    guard->_compactionInProgress = true;
    LOG_CTX("28d7d", TRACE, loggerContext)
        << "starting compaction on range " << compactionRange;
    auto f = store->removeFront(index);
    guard.unlock();
    std::move(f).thenFinal(
        [weak = weak_from_this(), promises = std::move(promises),
         reason = reason,
         compactionRange](futures::Try<Result> const& tryResult) mutable {
          auto self = weak.lock();
          if (self == nullptr) {
            return;
          }

          auto result = basics::catchToResult([&] { return tryResult.get(); });
          auto cresult = CompactResult{.stopReason = reason,
                                       .compactedRange = compactionRange};
          {
            auto guard = self->guarded.getLockedGuard();
            ADB_PROD_ASSERT(guard->status.inProgress.has_value());
            guard->status.lastCompaction = guard->status.inProgress;
            guard->status.inProgress.reset();
            if (result.fail()) {
              LOG_CTX("aa739", ERR, self->loggerContext)
                  << "error during compaction on range " << compactionRange
                  << ": " << result;
              cresult.error = std::move(result).error();
              guard->status.lastCompaction->error = cresult.error;
            } else {
              LOG_CTX("1ffec", TRACE, self->loggerContext)
                  << "compaction completed on range " << compactionRange;
              self->checkCompaction(std::move(guard));
            }
          }

          promises.resolveAll(futures::Try<CompactResult>{std::move(cresult)});
        });
  } else {
    LOG_CTX("35f56", TRACE, loggerContext)
        << "stopping compaction, reason = " << reason << " index = " << index
        << " log-range = " << logBounds;
    guard->_compactionInProgress = false;
    guard->status.stop = reason;
    guard.unlock();
    auto cresult = CompactResult{.stopReason = reason};
    promises.resolveAll(futures::Try<CompactResult>{std::move(cresult)});
  }
}

void CompactionManager::triggerAsyncCompaction(
    Guarded<GuardedData>::mutex_guard_type guard, bool ignoreThreshold) {
  guard->_fullCompactionNextRound |= ignoreThreshold;
  if (not guard->_compactionInProgress) {
    checkCompaction(std::move(guard));
  } else {
    LOG_CTX("b6135", TRACE, loggerContext)
        << "another compaction is still in progress";
  }
}
