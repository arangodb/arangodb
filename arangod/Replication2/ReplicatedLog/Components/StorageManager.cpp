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
#include "StorageManager.h"

#include <Futures/Future.h>
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/coro-helper.h"

using namespace arangodb::replication2::replicated_log;

struct comp::StorageManagerTransaction : IStorageTransaction {
  using GuardType = Guarded<StorageManager::GuardedData>::mutex_guard_type;

  auto getInMemoryLog() const noexcept -> InMemoryLog override {
    return guard->inMemoryLog;
  }
  auto getLogBounds() const noexcept -> LogRange override {
    return guard->inMemoryLog.getIndexRange();
  }

  auto removeFront(LogIndex stop) noexcept -> futures::Future<Result> override {
    struct RemoveFrontAction {};
    return Result{};
  }

  auto appendEntries(LogIndex appendAfter, InMemoryLogSlice slice) noexcept
      -> futures::Future<Result> override {
    return Result{};
  }

  explicit StorageManagerTransaction(GuardType guard)
      : guard(std::move(guard)) {}

  GuardType guard;
};

StorageManager::StorageManager(std::unique_ptr<IStorageEngineMethods> core)
    : guardedData(std::move(core)) {}

auto StorageManager::resign() -> std::unique_ptr<IStorageEngineMethods> {
  auto guard = guardedData.getLockedGuard();
  return nullptr;
}

StorageManager::GuardedData::GuardedData(
    std::unique_ptr<IStorageEngineMethods> core)
    : core(std::move(core)) {}
