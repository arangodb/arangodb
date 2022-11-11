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

using namespace arangodb::replication2::replicated_log::comp;

CompactionManager::CompactionManager(
    IStorageManager& storage, ISchedulerInterface& scheduler,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options)
    : guarded(storage, scheduler), options(std::move(options)) {}

void CompactionManager::updateReleaseIndex(LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  guard->releaseIndex = std::max(guard->releaseIndex, index);
  triggerAsyncCompaction(std::move(guard), false);
}

void CompactionManager::updateLargestIndexToKeep(LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  guard->largestIndexToKeep = std::max(guard->largestIndexToKeep, index);
  triggerAsyncCompaction(std::move(guard), false);
}

auto CompactionManager::compact() noexcept -> futures::Future<CompactResult> {
  auto guard = guarded.getLockedGuard();
  auto f = guard->compactAggregator.waitFor();
  triggerAsyncCompaction(std::move(guard), true);
  return f;
}

auto CompactionManager::getCompactionStatus() const noexcept
    -> CompactionStatus {
  return guarded.getLockedGuard()->status;
}

CompactionManager::GuardedData::GuardedData(IStorageManager& storage,
                                            ISchedulerInterface& scheduler)
    : storage(storage), scheduler(scheduler) {}

auto CompactionManager::GuardedData::isCompactionInProgress() const noexcept
    -> bool {
  return _compactionInProgress;
}

auto CompactionManager::calculateCompactionIndex(GuardedData const& data,
                                                 LogRange bounds,
                                                 bool ignoreThreshold) const
    -> std::tuple<LogIndex, CompactionStopReason> {
  auto [first, last] = bounds;
  auto newCompactionIndex =
      std::min(data.releaseIndex, data.largestIndexToKeep);
  auto nextAutomaticCompactionAt = first + options->_thresholdLogCompaction;
  if (not ignoreThreshold and nextAutomaticCompactionAt > newCompactionIndex) {
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

  auto [index, reason] = calculateCompactionIndex(
      guard.get(), logBounds, guard->_fullCompactionNextRound);

  if (index > logBounds.from) {
    auto& compaction = guard->status.inProgress.emplace();
    compaction.time = CompactionStatus::clock ::now();
    compaction.range = LogRange{logBounds.from, index};
    guard->_compactionInProgress = true;
    store->removeFront(index);
  } else {
    guard->status.stop = reason;
  }
}

void CompactionManager::triggerAsyncCompaction(
    Guarded<GuardedData>::mutex_guard_type guard, bool ignoreThreshold) {
  guard->_fullCompactionNextRound |= ignoreThreshold;
  if (not guard->_compactionInProgress) {
    checkCompaction(std::move(guard));
  }
}
