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

#include "InMemoryLog.h"

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

InMemoryLog::InMemoryLog(ParticipantId id, std::shared_ptr<InMemoryState> state)
    : _id(id), _state(std::move(state)), _commitIndex{LogIndex{0}} {}

auto InMemoryLog::appendEntries(AppendEntriesRequest)
    -> arangodb::futures::Future<AppendEntriesResult> {
  // TODO
  std::abort();
}

auto InMemoryLog::insert(LogPayload payload) -> LogIndex {
  auto const index = nextIndex();
  _log.emplace_back(LogEntry{_currentTerm, index, std::move(payload)});
  return index;
}

LogIndex InMemoryLog::nextIndex() { return LogIndex{_log.size() + 1}; }

auto InMemoryLog::createSnapshot()
    -> std::pair<LogIndex, std::shared_ptr<InMemoryState const>> {
  // TODO
  std::abort();
}

auto InMemoryLog::waitFor(LogIndex index) -> futures::Future<arangodb::futures::Unit> {
  return _waitForQueue.emplace(index, WaitForPromise{})->second.getFuture();
}

auto InMemoryLog::becomeFollower(LogTerm, ParticipantId) -> void {
  // TODO
  std::abort();
}

auto InMemoryLog::becomeLeader(LogTerm term, std::unordered_set<ParticipantId> followerIds,
                               std::size_t writeConcern) -> void {
  _role = Leader{std::move(followerIds), writeConcern};
}

[[nodiscard]] auto InMemoryLog::getStatistics() const -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = LogIndex{_log.size()};
  return result;
}

auto InMemoryLog::runAsyncStep() -> void {
  if (_log.size() > _commitIndex.value) {
    ++_commitIndex.value;
  }

  auto const end = _waitForQueue.upper_bound(_commitIndex);
  for (auto it = _waitForQueue.begin(); it != end; ++it) {
    it->second.setValue();
  }
}
