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

#include "Common.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;

auto LogIndex::operator==(LogIndex other) const -> bool {
  return this->value == other.value;
}
auto LogIndex::operator<=(LogIndex other) const -> bool {
  return value <= other.value;
}
auto LogIndex::operator<(LogIndex other) const -> bool {
  return value < other.value;
}

LogEntry::LogEntry(LogTerm logTerm, LogIndex logIndex, LogPayload payload)
    : _logTerm{logTerm}, _logIndex{logIndex}, _payload{std::move(payload)} {}

LogTerm LogEntry::logTerm() const { return _logTerm; }

LogIndex LogEntry::logIndex() const { return _logIndex; }

LogPayload const& LogEntry::logPayload() const { return _payload; }

