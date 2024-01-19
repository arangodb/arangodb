////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "InMemoryLogEntry.h"

namespace arangodb::replication2 {

InMemoryLogEntry::InMemoryLogEntry(LogEntry entry, bool waitForSync)
    : _waitForSync(waitForSync), _logEntry(std::move(entry)) {}

void InMemoryLogEntry::setInsertTp(clock::time_point tp) noexcept {
  _insertTp = tp;
}

auto InMemoryLogEntry::insertTp() const noexcept -> clock::time_point {
  return _insertTp;
}

auto InMemoryLogEntry::entry() const noexcept -> LogEntry const& {
  // Note that while get() isn't marked as noexcept, it actually is.
  return _logEntry.get();
}

}  // namespace arangodb::replication2
