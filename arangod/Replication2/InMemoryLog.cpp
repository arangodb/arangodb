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

#include "Basics/Exceptions.h"

#include <Basics/application-exit.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;

class InMemoryLogIterator : public LogIterator {
 public:
  explicit InMemoryLogIterator(std::deque<LogEntry>::const_iterator begin,
                               std::deque<LogEntry>::const_iterator end)
      : _begin(std::move(begin)), _end(std::move(end)) {}

  auto next() -> std::optional<LogEntry> override {
    if (_begin != _end) {
      auto const& res = *_begin;
      ++_begin;
      return res;
    }
    return std::nullopt;
  }

 private:
  std::deque<LogEntry>::const_iterator _begin;
  std::deque<LogEntry>::const_iterator _end;
};

InMemoryLog::InMemoryLog(ParticipantId id, std::shared_ptr<InMemoryState> state,
                         std::shared_ptr<PersistedLog> persistedLog)
    : _id(id), _persistedLog(std::move(persistedLog)), _state(std::move(state)), _commitIndex{LogIndex{0}} {
  if (ADB_UNLIKELY(_persistedLog == nullptr)) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "When instantiating InMemoryLog: persistedLog must not be a nullptr");
  }
}

auto InMemoryLog::appendEntries(AppendEntriesRequest)
    -> arangodb::futures::Future<AppendEntriesResult> {
  assertFollower();

  // TODO
  std::abort();
}

auto InMemoryLog::insert(LogPayload payload) -> LogIndex {
  assertLeader();
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
  _currentTerm = term;
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

  persistRemainingLogEntries();
}

void InMemoryLog::persistRemainingLogEntries() {
  if (_persistedLogEnd < nextIndex()) {
    auto from = _log.cbegin();
    auto const endIdx = nextIndex();
    TRI_ASSERT(_persistedLogEnd < endIdx);
    std::advance(from, _persistedLogEnd.value);
    auto to = _log.cend();
    auto it = std::make_shared<InMemoryLogIterator>(from, to);
    auto res = _persistedLog->insert(std::move(it));
    if (res.ok()) {
      _persistedLogEnd = endIdx;
    } else {
      LOG_TOPIC("c2bb2", INFO, Logger::REPLICATION)
          << "Error persisting log entries: " << res.errorMessage();
    }
  }
}

void InMemoryLog::assertLeader() const {
  if (ADB_UNLIKELY(!std::holds_alternative<Leader>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
  }
}

void InMemoryLog::assertFollower() const {
  if (ADB_UNLIKELY(!std::holds_alternative<Follower>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
  }
}
auto InMemoryLog::participantId() const noexcept -> ParticipantId { return _id; }
