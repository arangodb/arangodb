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
#include <Basics/overload.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
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
    : _joermungandr(id, std::move(state), std::move(persistedLog), LogIndex{0}) {
  auto self = acquireMutex();
  if (ADB_UNLIKELY(self->_persistedLog == nullptr)) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "When instantiating InMemoryLog: persistedLog must not be a nullptr");
  }
}

auto InMemoryLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto self = acquireMutex();
  return self->appendEntries(std::move(req));
}

auto InMemoryLog::GuardedInMemoryLog::appendEntries(AppendEntriesRequest req)
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
  auto self = acquireMutex();
  return self->insert(std::move(payload));
}
auto InMemoryLog::GuardedInMemoryLog::insert(LogPayload payload) -> LogIndex {
  assertLeader();
  auto const index = nextIndex();
  _log.emplace_back(LogEntry{_currentTerm, index, std::move(payload)});
  return index;
}

auto InMemoryLog::getStatus() const -> LogStatus {
  auto self = acquireMutex();
  return std::visit(overload{[&](Unconfigured const&) {
                               return LogStatus{UnconfiguredStatus{}};
                             },
                             [&](LeaderConfig const& leader) {
                               LeaderStatus status;
                               status.local = self->getStatistics();
                               for (auto const& f : leader.follower) {
                                 status.follower[f._impl->participantId()] =
                                     LogStatistics{f.lastAckedIndex, f.lastAckedCommitIndex};
                               }
                               return LogStatus{std::move(status)};
                             },
                             [&](FollowerConfig const& follower) {
                               FollowerStatus status;
                               status.local = self->getStatistics();
                               status.leader = follower.leaderId;
                               return LogStatus{std::move(status)};
                             }},
                    self->_role);
}

LogIndex InMemoryLog::GuardedInMemoryLog::nextIndex() const {
  return LogIndex{_log.size() + 1};
}
LogIndex InMemoryLog::GuardedInMemoryLog::getLastIndex() const {
  return LogIndex{_log.size()};
}

auto InMemoryLog::createSnapshot()
    -> std::pair<LogIndex, std::shared_ptr<InMemoryState const>> {
  auto self = acquireMutex();
  return self->createSnapshot();
}
auto InMemoryLog::GuardedInMemoryLog::createSnapshot()
    -> std::pair<LogIndex, std::shared_ptr<InMemoryState const>> {
  return std::make_pair(_commitIndex, _state->createSnapshot());
}

auto InMemoryLog::waitFor(LogIndex index) -> futures::Future<std::shared_ptr<QuorumData>> {
  auto self = acquireMutex();
  return self->waitFor(index);
}

auto InMemoryLog::GuardedInMemoryLog::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  assertLeader();
  auto it = _waitForQueue.emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

auto InMemoryLog::becomeFollower(LogTerm term, ParticipantId id) -> void {
  auto self = acquireMutex();
  return self->becomeFollower(term, id);
}
auto InMemoryLog::GuardedInMemoryLog::becomeFollower(LogTerm term, ParticipantId id) -> void {
  TRI_ASSERT(_currentTerm < term);
  _currentTerm = term;
  _role = FollowerConfig{id};
}

auto InMemoryLog::becomeLeader(LogTerm term,
                               std::vector<std::shared_ptr<LogFollower>> const& follower,
                               std::size_t writeConcern) -> void {
  auto self = acquireMutex();
  return self->becomeLeader(term, follower, writeConcern);
}

auto InMemoryLog::GuardedInMemoryLog::becomeLeader(
    LogTerm term, std::vector<std::shared_ptr<LogFollower>> const& follower,
    std::size_t writeConcern) -> void {
  TRI_ASSERT(_currentTerm < term);
  std::vector<Follower> follower_vec;
  follower_vec.reserve(follower.size());
  std::transform(follower.cbegin(), follower.cend(), std::back_inserter(follower_vec),
                 [&](std::shared_ptr<LogFollower> const& impl) -> Follower {
                   return Follower{impl};
                 });

  _role = LeaderConfig{std::move(follower_vec), writeConcern};
  _currentTerm = term;
}

[[nodiscard]] auto InMemoryLog::getLocalStatistics() const -> LogStatistics {
  auto self = acquireMutex();
  return self->getStatistics();
}
[[nodiscard]] auto InMemoryLog::GuardedInMemoryLog::getStatistics() const -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = LogIndex{_log.size()};
  return result;
}

auto InMemoryLog::runAsyncStep() -> void {
  auto self = acquireMutex();
  return self->runAsyncStep();
}

auto InMemoryLog::GuardedInMemoryLog::runAsyncStep() -> void {
  assertLeader();
  auto& conf = std::get<LeaderConfig>(_role);
  for (auto& follower : conf.follower) {
    sendAppendEntries(follower);
  }

  /*if (_log.size() > _commitIndex.value) {
    ++_commitIndex.value;
  }*/

  persistRemainingLogEntries();

  /**/
}

void InMemoryLog::GuardedInMemoryLog::persistRemainingLogEntries() {
  if (_persistedLogEnd < nextIndex()) {
    auto it = getLogIterator(_persistedLogEnd);
    auto const endIdx = getLastIndex();
    auto res = _persistedLog->insert(std::move(it));
    if (res.ok()) {
      _persistedLogEnd = endIdx;
      checkCommitIndex();
    } else {
      LOG_TOPIC("c2bb2", INFO, Logger::REPLICATION)
          << "Error persisting log entries: " << res.errorMessage();
    }
  }
}

void InMemoryLog::GuardedInMemoryLog::assertLeader() const {
  if (ADB_UNLIKELY(!std::holds_alternative<LeaderConfig>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
  }
}

void InMemoryLog::GuardedInMemoryLog::assertFollower() const {
  if (ADB_UNLIKELY(!std::holds_alternative<FollowerConfig>(_role))) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
  }
}

auto InMemoryLog::participantId() const noexcept -> ParticipantId {
  auto self = acquireMutex();
  return self->participantId();
}

auto InMemoryLog::GuardedInMemoryLog::participantId() const noexcept -> ParticipantId {
  return _id;
}

auto InMemoryLog::getEntryByIndex(LogIndex idx) const -> std::optional<LogEntry> {
  auto self = acquireMutex();
  return self->getEntryByIndex(idx);
}

auto InMemoryLog::GuardedInMemoryLog::getEntryByIndex(LogIndex idx) const
    -> std::optional<LogEntry> {
  if (_log.size() < idx.value || idx.value == 0) {
    return std::nullopt;
  }

  auto const& e = _log.at(idx.value - 1);
  TRI_ASSERT(e.logIndex() == idx);
  return e;
}

void InMemoryLog::GuardedInMemoryLog::updateCommitIndexLeader(LogIndex newIndex,
                                                              std::shared_ptr<QuorumData> quorum) {
  TRI_ASSERT(_commitIndex < newIndex);
  _commitIndex = newIndex;
  _lastQuorum = quorum;
  auto const end = _waitForQueue.upper_bound(_commitIndex);
  for (auto it = _waitForQueue.begin(); it != end; it = _waitForQueue.erase(it)) {
    it->second.setValue(quorum);
  }
}

void InMemoryLog::GuardedInMemoryLog::sendAppendEntries(Follower& follower) {
  if (follower.requestInFlight) {
    return;  // wait for the request to return
  }

  auto currentCommitIndex = _commitIndex;
  auto lastIndex = getLastIndex();
  if (follower.lastAckedIndex == lastIndex && _commitIndex == follower.lastAckedCommitIndex) {
    return;  // nothing to replicate
  }

  auto const lastAcked = getEntryByIndex(follower.lastAckedIndex);

  AppendEntriesRequest req;
  req.leaderCommit = _commitIndex;
  req.leaderTerm = _currentTerm;
  req.leaderId = _id;

  if (lastAcked) {
    req.prevLogIndex = lastAcked->logIndex();
    req.prevLogTerm = lastAcked->logTerm();
  } else {
    req.prevLogIndex = LogIndex{0};
    req.prevLogTerm = LogTerm{0};
  }

  // TODO maybe put an iterator into the request?
  auto it = getLogIterator(follower.lastAckedIndex);
  while (auto entry = it->next()) {
    req.entries.emplace_back(*std::move(entry));
  }

  follower.requestInFlight = true;
  follower._impl->appendEntries(std::move(req))
      .thenFinal([this, &follower, lastIndex,
                  currentCommitIndex](futures::Try<AppendEntriesResult>&& res) {
        TRI_ASSERT(res.hasValue());
        follower.requestInFlight = false;
        auto& response = res.get();
        if (response.success) {
          follower.lastAckedIndex = lastIndex;
          follower.lastAckedCommitIndex = currentCommitIndex;
          checkCommitIndex();
        }
        // try sending the next batch
        sendAppendEntries(follower);
      });
}

auto InMemoryLog::GuardedInMemoryLog::getLogIterator(LogIndex fromIdx)
    -> std::shared_ptr<LogIterator> {
  auto from = _log.cbegin();
  auto const endIdx = nextIndex();
  TRI_ASSERT(fromIdx < endIdx);
  std::advance(from, fromIdx.value);
  auto to = _log.cend();
  return std::make_shared<InMemoryLogIterator>(from, to);
}

void InMemoryLog::GuardedInMemoryLog::checkCommitIndex() {
  auto& conf = std::get<LeaderConfig>(_role);

  auto quorum_size = conf.writeConcern;

  // TODO make this so that we can place any predicate here
  std::vector<std::pair<LogIndex, ParticipantId>> indexes;
  std::transform(conf.follower.begin(), conf.follower.end(),
                 std::back_inserter(indexes), [](Follower const& f) {
                   return std::make_pair(f.lastAckedIndex, f._impl->participantId());
                 });
  TRI_ASSERT(_persistedLogEnd.value > 0);
  indexes.emplace_back(_persistedLogEnd, participantId());
  TRI_ASSERT(indexes.size() == conf.follower.size() + 1);

  if (quorum_size <= 0 || quorum_size > indexes.size()) {
    return;
  }

  std::nth_element(indexes.begin(), indexes.begin() + (quorum_size - 1), indexes.end(),
                   [](auto& a, auto& b) { return a.first > b.first; });

  auto& [commitIndex, participant] = indexes.at(quorum_size - 1);
  TRI_ASSERT(commitIndex >= _commitIndex);
  if (commitIndex > _commitIndex) {
    std::vector<ParticipantId> quorum;
    std::transform(indexes.begin(), indexes.begin() + quorum_size, std::back_inserter(quorum),
                   [](auto& p) { return p.second; });

    auto quorum_data =
        std::make_shared<QuorumData>(commitIndex, _currentTerm, std::move(quorum));
    updateCommitIndexLeader(commitIndex, std::move(quorum_data));
  }
}

auto InMemoryLog::acquireMutex()
    -> MutexGuard<GuardedInMemoryLog, std::unique_lock<std::mutex>> {
  return _joermungandr.getLockedGuard();
}
auto InMemoryLog::acquireMutex() const
    -> MutexGuard<GuardedInMemoryLog const, std::unique_lock<std::mutex>> {
  return _joermungandr.getLockedGuard();
}

auto InMemoryState::createSnapshot() -> std::shared_ptr<InMemoryState const> {
  return std::make_shared<InMemoryState>(_state);
}

InMemoryState::InMemoryState(InMemoryState::state_container state)
    : _state(std::move(state)) {}

void DelayedFollowerLog::runAsyncAppendEntries() {
  auto asyncQueue = std::move(_asyncQueue);
  _asyncQueue.clear();

  for (auto& p : asyncQueue) {
    p.setValue();
  }
}
auto DelayedFollowerLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto f = _asyncQueue.emplace_back().getFuture();

  return std::move(f).then([this, req = std::move(req)](auto) mutable {
    return InMemoryLog::appendEntries(std::move(req));
  });
}

QuorumData::QuorumData(const LogIndex& index, LogTerm term, std::vector<ParticipantId> quorum)
    : index(index), term(term), quorum(std::move(quorum)) {}

void AppendEntriesResult::toVelocyPack(velocypack::Builder& builder) const {
  {
    velocypack::ObjectBuilder ob(&builder);
    builder.add("term", VPackValue(logTerm.value));
    builder.add("success", VPackValue(success));
  }
}

auto AppendEntriesResult::fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult {
  bool success = slice.get("success").getBool();
  auto logTerm = LogTerm{slice.get("term").getNumericValue<size_t>()};

  return AppendEntriesResult{success, logTerm};
}

void AppendEntriesRequest::toVelocyPack(velocypack::Builder& builder) const {
  {
    velocypack::ObjectBuilder ob(&builder);
    builder.add("leaderTerm", VPackValue(leaderTerm.value));
    builder.add("leaderId", VPackValue(leaderId));
    builder.add("prevLogTerm", VPackValue(prevLogTerm.value));
    builder.add("prevLogIndex", VPackValue(prevLogIndex.value));
    builder.add("leaderCommit", VPackValue(leaderCommit.value));
    builder.add("entries", VPackValue(VPackValueType::Array));
    for (auto const& it : entries) {
      it.toVelocyPack(builder);
    }
    builder.close();  // close entries
  }
}

auto AppendEntriesRequest::fromVelocyPack(velocypack::Slice slice) -> AppendEntriesRequest {
  auto leaderTerm = LogTerm{slice.get("leaderTerm").getNumericValue<size_t>()};
  auto leaderId = ParticipantId{slice.get("leaderId").copyString()};
  auto prevLogTerm = LogTerm{slice.get("prevLogTerm").getNumericValue<size_t>()};
  auto prevLogIndex = LogIndex{slice.get("prevLogIndex").getNumericValue<size_t>()};
  auto leaderCommit = LogIndex{slice.get("leaderCommit").getNumericValue<size_t>()};
  auto entries = std::invoke([&] {
    auto entriesVp = velocypack::ArrayIterator(slice.get("entries"));
    auto entries = std::vector<LogEntry>{};
    entries.reserve(entriesVp.size());
    std::transform(entriesVp.begin(), entriesVp.end(), std::back_inserter(entries),
                   [](auto const& it) { return LogEntry::fromVelocyPack(it); });
    return entries;
  });

  return AppendEntriesRequest{leaderTerm,   leaderId,     prevLogTerm,
                              prevLogIndex, leaderCommit, std::move(entries)};
}
