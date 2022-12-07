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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "LogCore.h"

#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"

#include <Basics/Exceptions.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

replicated_log::LogCore::LogCore(
    replicated_state::IStorageEngineMethods& methods)
    : _storage(methods) {}

auto replicated_log::LogCore::removeBack(LogIndex first) -> Result {
  std::unique_lock guard(_operationMutex);
  return _storage.removeBack(first, {}).get().result();
}

auto replicated_log::LogCore::read(LogIndex first) const
    -> std::unique_ptr<PersistedLogIterator> {
  std::unique_lock guard(_operationMutex);
  return _storage.read(first);
}

auto replicated_log::LogCore::insertAsync(
    std::unique_ptr<PersistedLogIterator> iter, bool waitForSync)
    -> futures::Future<Result> {
  std::unique_lock guard(_operationMutex);
  // This will hold the mutex
  IStorageEngineMethods::WriteOptions opts;
  opts.waitForSync = waitForSync;
  return _storage.insert(std::move(iter), opts)
      .thenValue(
          [guard = std::move(guard)](
              ResultT<IStorageEngineMethods::SequenceNumber>&& res) mutable {
            guard.unlock();
            return std::move(res).result();
          });
}

auto replicated_log::LogCore::logId() const noexcept -> LogId {
  return _storage.getLogId();
}

auto LogCore::removeFront(LogIndex stop) -> futures::Future<Result> {
  std::unique_lock guard(_operationMutex);
  return _storage.removeFront(stop, {}).thenValue(
      [guard = std::move(guard)](
          ResultT<IStorageEngineMethods::SequenceNumber>&& res) mutable {
        guard.unlock();
        return std::move(res).result();
      });
}

auto LogCore::updateSnapshotState(replicated_state::SnapshotStatus status)
    -> Result {
  auto metaResult = _storage.readMetadata();
  if (metaResult.fail()) {
    return metaResult.result();
  }
  auto& meta = metaResult.get();
  meta.snapshot.status = status;
  return _storage.updateMetadata(meta);
}

auto LogCore::getSnapshotState() -> ResultT<replicated_state::SnapshotStatus> {
  auto metaResult = _storage.readMetadata();
  if (metaResult.fail()) {
    return metaResult.result();
  }
  return metaResult->snapshot.status;
}

void LogCore::waitForCompletion() noexcept { _storage.waitForCompletion(); }
