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

using namespace arangodb::replication2::replicated_log::comp;

CompactionManager::CompactionManager(IStorageManager& storage)
    : guarded(storage) {}

void CompactionManager::updateReleaseIndex(
    arangodb::replication2::LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  guard->releaseIndex = std::max(guard->releaseIndex, index);
  guard->triggerAsyncCompaction();
}

void CompactionManager::updateLargestIndexToKeep(
    arangodb::replication2::LogIndex index) noexcept {
  auto guard = guarded.getLockedGuard();
  guard->largestIndexToKeep = std::max(guard->largestIndexToKeep, index);
  guard->triggerAsyncCompaction();
}

auto CompactionManager::compact() noexcept
    -> futures::Future<arangodb::ResultT<CompactionResult>> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto CompactionManager::getCompactionStatus() const noexcept
    -> arangodb::replication2::replicated_log::CompactionStatus {
  return guarded.getLockedGuard()->lastCompaction;
}

CompactionManager::GuardedData::GuardedData(IStorageManager& storage)
    : storage(storage) {}

void CompactionManager::GuardedData::triggerAsyncCompaction() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
