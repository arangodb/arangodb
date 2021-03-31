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

template <typename I>
struct ContainerIterator : LogIterator {
  static_assert(std::is_same_v<typename I::value_type, LogEntry>);

  ContainerIterator(I begin, I end) : _current(begin), _end(end) {}

  auto next() -> std::optional<LogEntry> override {
    if (_current == _end) {
      return std::nullopt;
    }
    return *(_current++);
  }

  I _current;
  I _end;
};

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

auto InMemoryLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  assertFollower();

  // TODO does >= suffice here? Maybe we want to do an atomic operation
  // before increasing our term
  if (req.leaderTerm != _currentTerm) {
    return AppendEntriesResult{false, _currentTerm};
  }

  if (req.prevLogIndex > LogIndex{0}) {
    auto entry = getEntryByIndex(req.prevLogIndex);
    if (!entry.has_value() || entry->logTerm() != req.prevLogTerm) {
      return AppendEntriesResult{false, _currentTerm};
    }
  }

  auto res = _persistedLog->removeBack(req.prevLogIndex + 1);
  if (!res.ok()) {
    abort();
  }

  auto iter = std::make_shared<ContainerIterator<std::vector<LogEntry>::const_iterator>>(
      req.entries.cbegin(), req.entries.cend());
  res = _persistedLog->insert(iter);
  if (!res.ok()) {
    abort();
  }

  _log.erase(_log.begin() + req.prevLogIndex.value, _log.end());
  _log.insert(_log.end(), req.entries.begin(), req.entries.end());

  if (_commitIndex < req.leaderCommit && !_log.empty()) {
    _commitIndex = std::min(req.leaderCommit, _log.back().logIndex());
    // TODO Apply operations to state machine here?
  }

  return AppendEntriesResult{true, _currentTerm};
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
  return std::make_pair(_commitIndex, _state->createSnapshot());
}

auto InMemoryLog::waitFor(LogIndex index) -> futures::Future<arangodb::futures::Unit> {
  auto it = _waitForQueue.emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

auto InMemoryLog::becomeFollower(LogTerm term, ParticipantId id) -> void {
  TRI_ASSERT(_currentTerm < term);
  _currentTerm = term;
  _role = FollowerConfig{id};
}

auto InMemoryLog::becomeLeader(LogTerm term,
                               std::vector<std::shared_ptr<LogFollower>> const& follower,
                               std::size_t writeConcern) -> void {
  TRI_ASSERT(_currentTerm < term);
  std::vector<Follower> follower_vec;
  follower_vec.reserve(follower.size());
  std::transform(follower.cbegin(), follower.cend(), std::back_inserter(follower_vec),
                 [&](std::shared_ptr<LogFollower> const& impl) -> Follower {
                   return Follower{impl, LogIndex{0}};
                 });

  _role = LeaderConfig{std::move(follower_vec), writeConcern};
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
  if (ADB_UNLIKELY(!std::holds_alternative<LeaderConfig>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
  }
}

void InMemoryLog::assertFollower() const {
  if (ADB_UNLIKELY(!std::holds_alternative<FollowerConfig>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
  }
}
auto InMemoryLog::participantId() const noexcept -> ParticipantId {
  return _id;
}

auto InMemoryState::createSnapshot() -> std::shared_ptr<InMemoryState const> {
  return std::make_shared<InMemoryState>(_state);
}

auto InMemoryLog::getEntryByIndex(LogIndex idx) const -> std::optional<LogEntry> {
  if (_log.size() < idx.value || idx.value == 0) {
    return std::nullopt;
  }

  auto const& e = _log.at(idx.value - 1);
  TRI_ASSERT(e.logIndex() == idx);
  return e;
}

InMemoryState::InMemoryState(InMemoryState::state_container state)
    : _state(std::move(state)) {}
