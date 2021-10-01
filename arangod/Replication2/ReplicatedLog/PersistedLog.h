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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGODB3_PERSISTEDLOG_H
#define ARANGODB3_PERSISTEDLOG_H

#include "Replication2/ReplicatedLog/LogCommon.h"

#include "Basics/Result.h"
#include "Futures/Future.h"

#include <memory>

namespace arangodb::replication2::replicated_log {

// ReplicatedLog-internal iterator over PersistingLogEntries
struct PersistedLogIterator : TypedLogIterator<PersistingLogEntry> {};

/**
 * @brief Interface to persist a replicated log locally. Implemented by
 * arango::RocksDBLog.
 */
struct PersistedLog {
  virtual ~PersistedLog() = default;
  explicit PersistedLog(LogId lid) : _lid(lid) {}

  struct WriteOptions {
    bool waitForSync = false;
  };

  [[nodiscard]] auto id() const noexcept -> LogId { return _lid; }
  virtual auto insert(PersistedLogIterator& iter, WriteOptions const&) -> Result = 0;
  virtual auto insertAsync(std::unique_ptr<PersistedLogIterator> iter, WriteOptions const&) -> futures::Future<Result> = 0;
  virtual auto read(LogIndex start) -> std::unique_ptr<PersistedLogIterator> = 0;
  virtual auto removeFront(LogIndex stop) -> Result = 0;
  virtual auto removeBack(LogIndex start) -> Result = 0;

  virtual auto drop() -> Result = 0;

 private:
  LogId _lid;
};

}  // namespace arangodb::replication2

#endif  // ARANGODB3_PERSISTEDLOG_H
