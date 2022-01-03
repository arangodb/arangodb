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

#include <Basics/Exceptions.h>
#include <Basics/debugging.h>
#include <Basics/system-compiler.h>
#include <Basics/voc-errors.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

replicated_log::LogCore::LogCore(std::shared_ptr<PersistedLog> persistedLog)
    : _persistedLog(std::move(persistedLog)) {
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

auto replicated_log::LogCore::insert(PersistedLogIterator& iter,
                                     bool waitForSync) -> Result {
  std::unique_lock guard(_operationMutex);
  PersistedLog::WriteOptions opts;
  opts.waitForSync = waitForSync;
  return _persistedLog->insert(iter, opts);
}

auto replicated_log::LogCore::read(LogIndex first) const
    -> std::unique_ptr<PersistedLogIterator> {
  std::unique_lock guard(_operationMutex);
  return _persistedLog->read(first);
}

auto replicated_log::LogCore::insertAsync(
    std::unique_ptr<PersistedLogIterator> iter, bool waitForSync)
    -> futures::Future<Result> {
  std::unique_lock guard(_operationMutex);
  // This will hold the mutex
  PersistedLog::WriteOptions opts;
  opts.waitForSync = waitForSync;
  return _persistedLog->insertAsync(std::move(iter), opts)
      .thenValue([guard = std::move(guard)](Result&& res) mutable {
        guard.unlock();
        return std::move(res);
      });
}

auto replicated_log::LogCore::releasePersistedLog() && -> std::shared_ptr<
    PersistedLog> {
  std::unique_lock guard(_operationMutex);
  return std::move(_persistedLog);
}

auto replicated_log::LogCore::logId() const noexcept -> LogId {
  return _persistedLog->id();
}

auto LogCore::removeFront(LogIndex stop) -> Result {
  std::unique_lock guard(_operationMutex);
  return _persistedLog->removeFront(stop);
}
