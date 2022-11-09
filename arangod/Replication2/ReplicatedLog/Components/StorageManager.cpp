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

using namespace arangodb::replication2::replicated_log::comp;

StorageManager::StorageManager(std::unique_ptr<LogCore> core)
    : guardedData(std::move(core)) {}

auto StorageManager::getInMemoryLog() const noexcept
    -> arangodb::replication2::replicated_log::InMemoryLog {
  return guardedData.getLockedGuard()->inMemoryLog;
}

auto StorageManager::removeFront(arangodb::replication2::LogIndex stop) noexcept
    -> arangodb::futures::Future<arangodb::Result> {
  auto [newLog, f] = guardedData.doUnderLock([stop](GuardedData& data) {
    auto newLog = data.inMemoryLog.release(stop);
    auto f = data.core->removeFront(stop);
    return std::make_pair(std::move(newLog), std::move(f));
  });
  return std::move(f).thenValue(
      [newLog = std::move(newLog), weak = weak_from_this()](
          ResultT<replicated_state::IStorageEngineMethods::SequenceNumber>
              result) mutable {
        if (auto self = weak.lock(); self) {
          auto guard = self->guardedData.getLockedGuard();
          guard->inMemoryLog = std::move(newLog);
        }

        return result.result();
      });
}

auto StorageManager::resign() -> std::unique_ptr<LogCore> {
  return std::move(guardedData.getLockedGuard()->core);
}

StorageManager::GuardedData::GuardedData(std::unique_ptr<LogCore> core)
    : core(std::move(core)) {}

auto StorageTransaction::removeFront(
    arangodb::replication2::LogIndex stop) noexcept
    -> arangodb::futures::Future<arangodb::Result> {
  return interface->removeFront(stop);
}

auto StorageTransaction::getInMemoryLog() const noexcept
    -> const arangodb::replication2::replicated_log::InMemoryLog& {
  return interface->getInMemoryLog();
}
