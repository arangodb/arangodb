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
#include "MockLog.h"

#include <Basics/debugging.h>
#include <Logger/LogMacros.h>

using namespace arangodb;
using namespace arangodb::replication2;

namespace std {
auto operator<<(std::ostream& ostream, LogIndex logIndex) -> std::ostream& {
  return ostream << "LogIndex{" << logIndex.value << "}";
}
auto operator<<(std::ostream& ostream, LogEntry const& logEntry) -> std::ostream& {
  return ostream << "LogEntry{" << logEntry.logTerm().value << ", " << logEntry.logIndex() << ", " << logEntry.logPayload().dummy << "}";
}
}

auto arangodb::MockLog::insert(std::shared_ptr<LogIterator> iter) -> arangodb::Result {
  while (auto entry = iter->next()) {
    auto const res = _storage.try_emplace(entry->logIndex(), entry.value());
    TRI_ASSERT(res.second);
  }

  return {};
}

template <typename I>
struct ContainerIterator : LogIterator {
  ContainerIterator(MockLog::storeType store, LogIndex start)
      : _store(std::move(store)),
        _current(_store.lower_bound(start)),
        _end(_store.end()) {}

  auto next() -> std::optional<LogEntry> override {
    if (_current == _end) {
      return std::nullopt;
    }
    return (_current++)->second;
  }

  MockLog::storeType _store;
  I _current;
  I _end;
};

auto arangodb::MockLog::read(arangodb::replication2::LogIndex start)
    -> std::shared_ptr<LogIterator> {
  return std::make_shared<ContainerIterator<iteratorType>>(_storage, start);
}

auto arangodb::MockLog::removeFront(arangodb::replication2::LogIndex stop)
    -> arangodb::Result {
  _storage.erase(_storage.begin(), _storage.lower_bound(stop));
  return {};
}

auto arangodb::MockLog::removeBack(arangodb::replication2::LogIndex start)
    -> arangodb::Result {
  _storage.erase(_storage.lower_bound(start), _storage.end());
  return {};
}

auto arangodb::MockLog::drop() -> arangodb::Result {
  _storage.clear();
  return Result();
}

MockLog::MockLog(replication2::LogId id) : PersistedLog(id) {}
MockLog::MockLog(replication2::LogId id, MockLog::storeType storage)
    : PersistedLog(id), _storage(std::move(storage)) {}
