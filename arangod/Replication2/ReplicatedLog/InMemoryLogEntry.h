////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Replication2/ReplicatedLog/LogEntry.h"

namespace arangodb::replication2 {

// A log entry, enriched with non-persisted metadata, to be stored in an
// InMemoryLog.
class InMemoryLogEntry {
 public:
  using clock = std::chrono::steady_clock;

  explicit InMemoryLogEntry(LogEntry entry, bool waitForSync = false);

  [[nodiscard]] auto insertTp() const noexcept -> clock::time_point;
  void setInsertTp(clock::time_point) noexcept;
  [[nodiscard]] auto entry() const noexcept -> LogEntry const&;
  [[nodiscard]] bool getWaitForSync() const noexcept { return _waitForSync; }

 private:
  bool _waitForSync;
  // Immutable box that allows sharing, i.e. cheap copying.
  ::immer::box<LogEntry, ::arangodb::immer::arango_memory_policy> _logEntry;
  // Timepoint at which the insert was started (not the point in time where it
  // was committed)
  clock::time_point _insertTp{};
};

using InMemoryLogIterator = TypedLogIterator<InMemoryLogEntry>;

}  // namespace arangodb::replication2
