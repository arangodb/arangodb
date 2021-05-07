////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Replication2/ReplicatedLog/Persistor.h"

#include <Basics/Exceptions.h>
#include <Basics/debugging.h>
#include <Basics/system-compiler.h>
#include <Basics/voc-errors.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;

replicated_log::LogCore::LogCore(std::shared_ptr<PersistedLog> persistedLog,
                                 std::shared_ptr<Persistor> persistor)
    : _persistedLog(std::move(persistedLog)), _persistor(std::move(persistor)) {
  if (ADB_UNLIKELY(_persistedLog == nullptr)) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "When instantiating ReplicatedLog: "
                                   "persistedLog must not be a nullptr");
  }
}

auto replicated_log::LogCore::removeBack(LogIndex first) -> Result {
  std::unique_lock guard(_operationMutex);
  return _persistedLog->removeBack(first);
}

auto replicated_log::LogCore::insert(LogIterator& iter) -> Result {
  std::unique_lock guard(_operationMutex);
  return _persistedLog->insert(iter);
}

auto replicated_log::LogCore::read(LogIndex first) -> std::unique_ptr<LogIterator> {
  std::unique_lock guard(_operationMutex);
  // TODO is this safe? Or do we have to hold the lock as long as the iterator exists?
  //      I think this is safe because of rocksdb but at this point its an implementation detail.
  return _persistedLog->read(first);
}

auto replicated_log::LogCore::insertAsync(std::unique_ptr<LogIterator> iter)
    -> futures::Future<Result> {
  std::unique_lock guard(_operationMutex);
  // This will hold the mutex
  return _persistor->persist(_persistedLog, std::move(iter))
      .thenValue([guard = std::move(guard)](Result&& res) mutable {
        guard.unlock();
        return std::move(res);
      });
}

auto replicated_log::LogCore::releasePersistedLog() && -> std::shared_ptr<PersistedLog> {
  std::unique_lock guard(_operationMutex);
  return std::move(_persistedLog);
}

auto replicated_log::LogCore::logId() const noexcept -> LogId {
  return _persistedLog->id();
}