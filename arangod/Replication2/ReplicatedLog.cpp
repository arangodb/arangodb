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

#include "ReplicatedLog.h"

#include "Basics/Exceptions.h"

#include <Basics/application-exit.h>
#include <Basics/overload.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <utility>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

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

class ReplicatedLogIterator : public LogIterator {
 public:
  explicit ReplicatedLogIterator(immer::flex_vector<LogEntry>::const_iterator begin,
                                 immer::flex_vector<LogEntry>::const_iterator end)
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
  immer::flex_vector<LogEntry>::const_iterator _begin;
  immer::flex_vector<LogEntry>::const_iterator _end;
};

namespace {
namespace follower {

auto appendEntries(replicated_log::LogParticipant& participant, AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  // TODO does >= suffice here? Maybe we want to do an atomic operation
  // before increasing our term
  if (req.leaderTerm != participant._currentTerm) {
    return AppendEntriesResult{false, participant._currentTerm};
  }

  if (req.prevLogIndex > LogIndex{0}) {
    auto entry = participant.getEntryByIndex(req.prevLogIndex);
    if (!entry.has_value() || entry->logTerm() != req.prevLogTerm) {
      return AppendEntriesResult{false, participant._currentTerm};
    }
  }

  auto res = participant._logCore->_persistedLog->removeBack(req.prevLogIndex + 1);
  if (!res.ok()) {
    abort();
  }

  auto iter = ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
      req.entries.begin(), req.entries.end());
  res = participant._logCore->_persistedLog->insert(iter);
  if (!res.ok()) {
    abort();
  }

  auto transientLog = participant._logCore->_log.transient();
  transientLog.take(req.prevLogIndex.value);
  transientLog.append(req.entries.transient());
  participant._logCore->_log = std::move(transientLog).persistent();

  if (participant._commitIndex < req.leaderCommit && !participant._logCore->_log.empty()) {
    participant._commitIndex =
        std::min(req.leaderCommit, participant._logCore->_log.back().logIndex());
    // TODO Apply operations to state machine here?
  }

  return AppendEntriesResult{true, participant._currentTerm};
}

}  // namespace follower
}  // namespace

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto participantGuard = acquireMutex();
  return follower::appendEntries(participantGuard.get(), std::move(req));
}

auto replicated_log::LogLeader::acquireMutex() -> LogLeader::Guard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::acquireMutex() const -> LogLeader::ConstGuard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::resign() && -> std::unique_ptr<LogCore> {
  return _guardedLeaderData.doUnderLock([](auto& leaderData) {
    return std::move(leaderData._participant._logCore);
  });
}

auto replicated_log::LogLeader::readReplicatedEntryByIndex(LogIndex idx) const
    -> std::optional<LogEntry> {
  return _guardedLeaderData.doUnderLock([&idx](auto& leaderData) -> std::optional<LogEntry> {
    if (auto entry = leaderData._participant.getEntryByIndex(idx);
        entry.has_value() && entry->logIndex() <= leaderData._participant._commitIndex) {
      return entry;
    } else {
      return std::nullopt;
    }
  });
}

auto replicated_log::LogFollower::acquireMutex() -> replicated_log::LogFollower::Guard {
  return _guardedParticipant.getLockedGuard();
}

auto replicated_log::LogFollower::acquireMutex() const
    -> replicated_log::LogFollower::ConstGuard {
  return _guardedParticipant.getLockedGuard();
}

auto replicated_log::LogFollower::getStatus() const -> LogStatus {
  return _guardedParticipant.doUnderLock([&leaderId = _leaderId](auto& participant) {
    FollowerStatus status;
    status.local = participant.getLocalStatistics();
    status.leader = leaderId;
    status.term = participant._currentTerm;
    return LogStatus{std::move(status)};
  });
}

auto replicated_log::LogFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return _guardedParticipant.doUnderLock([](auto const& participant) -> ParticipantId const& {
    return participant._id;
  });
}

auto replicated_log::LogFollower::resign() && -> std::unique_ptr<LogCore> {
  return _guardedParticipant.doUnderLock(
      [](auto& participant) { return std::move(participant._logCore); });
}

auto replicated_log::LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

replicated_log::LogUnconfiguredParticipant::LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore)
    : _logCore(std::move(logCore)) {}

auto replicated_log::LogUnconfiguredParticipant::resign() && -> std::unique_ptr<LogCore>{
  return std::move(_logCore);
}

auto replicated_log::LogLeader::getStatus() const -> LogStatus {
  return _guardedLeaderData.doUnderLock([](auto& leaderData) {
    LeaderStatus status;
    status.local = leaderData._participant.getLocalStatistics();
    status.term = leaderData._participant._currentTerm;
    for (auto const& f : leaderData._follower) {
      status.follower[f._impl->getParticipantId()] =
          LogStatistics{f.lastAckedIndex, f.lastAckedCommitIndex};
    }
    return LogStatus{std::move(status)};
  });
}

auto replicated_log::LogLeader::insert(LogPayload payload) -> LogIndex {
  auto self = acquireMutex();
  return self->insert(std::move(payload));
}

auto replicated_log::LogLeader::GuardedLeaderData::insert(LogPayload payload) -> LogIndex {
  // TODO this has to be lock free
  // TODO investigate what order between insert-increaseTerm is required?
  // Currently we use a mutex. Is this the only valid semantic?
  auto const index = _participant._logCore->getNextIndex();
  _participant._logCore->_log = _participant._logCore->_log.push_back(
      LogEntry{_participant._currentTerm, index, std::move(payload)});
  return index;
}

auto replicated_log::LogLeader::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  auto self = acquireMutex();
  return self->waitFor(index);
}

auto replicated_log::LogLeader::GuardedLeaderData::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  if (_participant._commitIndex >= index) {
    return futures::Future<std::shared_ptr<QuorumData>>{std::in_place, _lastQuorum};
  }
  auto it = _waitForQueue.emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

[[nodiscard]] auto replicated_log::LogParticipant::getLocalStatistics() const -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = _logCore->getLastIndex();
  return result;
}

auto replicated_log::LogLeader::getParticipantId() const noexcept -> ParticipantId const& {
  return _guardedLeaderData.doUnderLock([](GuardedLeaderData const& data) -> ParticipantId const& {
    return data._participant.participantId();
  });
}

auto replicated_log::LogLeader::runAsyncStep() -> void {
  auto guard = acquireMutex();
  return guard->runAsyncStep(weak_from_this());
}
auto replicated_log::LogLeader::GuardedLeaderData::runAsyncStep(std::weak_ptr<LogLeader> const& leader)
    -> void {
  for (auto& follower : _follower) {
    sendAppendEntries(leader, follower);
  }
}

auto replicated_log::LogParticipant::participantId() const noexcept -> ParticipantId const& {
  return _id;
}

auto replicated_log::LogParticipant::getEntryByIndex(LogIndex idx) const
    -> std::optional<LogEntry> {
  if (_logCore->_log.size() < idx.value || idx.value == 0) {
    return std::nullopt;
  }

  auto const& e = _logCore->_log.at(idx.value - 1);
  TRI_ASSERT(e.logIndex() == idx);
  return e;
}

void replicated_log::LogLeader::GuardedLeaderData::updateCommitIndexLeader(
    std::weak_ptr<LogLeader> const& parentLog, LogIndex newIndex,
    const std::shared_ptr<QuorumData>& quorum) {
  TRI_ASSERT(_participant._commitIndex < newIndex);
  _participant._commitIndex = newIndex;
  _lastQuorum = quorum;
  for (auto& follower : _follower) {
    sendAppendEntries(parentLog, follower);
  }
  auto const end = _waitForQueue.upper_bound(_participant._commitIndex);
  for (auto it = _waitForQueue.begin(); it != end; it = _waitForQueue.erase(it)) {
    it->second.setValue(quorum);
  }
}

void replicated_log::LogLeader::GuardedLeaderData::sendAppendEntries(
    std::weak_ptr<LogLeader> const& parentLog, Follower& follower) {
  if (follower.requestInFlight) {
    return;  // wait for the request to return
  }

  auto currentCommitIndex = _participant._commitIndex;
  auto lastIndex = _participant._logCore->getLastIndex();
  if (follower.lastAckedIndex == lastIndex &&
      _participant._commitIndex == follower.lastAckedCommitIndex) {
    return;  // nothing to replicate
  }

  auto const lastAcked = _participant.getEntryByIndex(follower.lastAckedIndex);

  AppendEntriesRequest req;
  req.leaderCommit = _participant._commitIndex;
  req.leaderTerm = _participant._currentTerm;
  req.leaderId = _participant._id;

  if (lastAcked) {
    req.prevLogIndex = lastAcked->logIndex();
    req.prevLogTerm = lastAcked->logTerm();
  } else {
    req.prevLogIndex = LogIndex{0};
    req.prevLogTerm = LogTerm{0};
  }

  {
    // TODO maybe put an iterator into the request?
    auto it = getLogIterator(follower.lastAckedIndex);
    auto transientEntries = immer::flex_vector_transient<LogEntry>{};
    while (auto entry = it->next()) {
      transientEntries.push_back(*std::move(entry));
    }
    req.entries = std::move(transientEntries).persistent();
  }

  // Capture self(shared_from_this()) instead of this
  // additionally capture a weak pointer that will be locked
  // when the request returns. If the locking is successful
  // we are still in the same term.
  follower.requestInFlight = true;
  follower._impl->appendEntries(std::move(req))
      .thenFinal([parentLog = parentLog, &follower, lastIndex,
                  currentCommitIndex, currentTerm = _participant._currentTerm](
                     futures::Try<AppendEntriesResult>&& res) {
        if (auto self = parentLog.lock()) {
          auto guarded = self->acquireMutex();
          guarded->handleAppendEntriesResponse(parentLog, follower, lastIndex, currentCommitIndex,
                                               currentTerm, std::move(res));
        }
      });
}

void replicated_log::LogLeader::GuardedLeaderData::handleAppendEntriesResponse(
    std::weak_ptr<LogLeader> const& parentLog, Follower& follower,
    LogIndex lastIndex, LogIndex currentCommitIndex, LogTerm currentTerm,
    futures::Try<AppendEntriesResult>&& res) {
  if (currentTerm != _participant._currentTerm) {
    return;
  }
  follower.requestInFlight = false;
  if (res.hasValue()) {
    follower.numErrorsSinceLastAnswer = 0;
    auto& response = res.get();
    if (response.success) {
      follower.lastAckedIndex = lastIndex;
      follower.lastAckedCommitIndex = currentCommitIndex;
      checkCommitIndex(parentLog);
    } else {
      // TODO Optimally, we'd like this condition (lastAckedIndex > 0) to be
      //      assertable here. For that to work, we need to make sure that no
      //      other failures than "I don't have that log entry" can lead to this
      //      branch.
      if (follower.lastAckedIndex > LogIndex{0}) {
        follower.lastAckedIndex = LogIndex{follower.lastAckedIndex.value - 1};
      }
    }
  } else if (res.hasException()) {
    auto const exp = follower.numErrorsSinceLastAnswer;
    ++follower.numErrorsSinceLastAnswer;

    using namespace std::chrono_literals;
    // Capped exponential backoff. Wait for 100us, 200us, 400us, ...
    // until at most 100us * 2 ** 17 == 13.11s.
    auto const sleepFor = 100us * (1 << std::min(exp, std::size_t{17}));

    std::this_thread::sleep_for(sleepFor);

    try {
      res.throwIfFailed();
    } catch (std::exception const& e) {
      LOG_TOPIC("e094b", INFO, Logger::REPLICATION2)
          << "exception in appendEntries to follower "
          << follower._impl->getParticipantId() << ": " << e.what();
    } catch (...) {
      LOG_TOPIC("05608", INFO, Logger::REPLICATION2)
          << "exception in appendEntries to follower "
          << follower._impl->getParticipantId() << ".";
    }
  } else {
    LOG_TOPIC("dc441", FATAL, Logger::REPLICATION2)
        << "in appendEntries to follower " << follower._impl->getParticipantId()
        << ", result future has neither value nor exception.";
    TRI_ASSERT(false);
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
  }
  // try sending the next batch
  sendAppendEntries(parentLog, follower);
}

auto replicated_log::LogLeader::GuardedLeaderData::getLogIterator(LogIndex fromIdx) const
    -> std::shared_ptr<LogIterator> {
  auto from = _participant._logCore->_log.begin();
  auto const endIdx = _participant._logCore->getNextIndex();
  TRI_ASSERT(fromIdx < endIdx);
  std::advance(from, fromIdx.value);
  auto to = _participant._logCore->_log.end();
  return std::make_shared<ReplicatedLogIterator>(from, to);
}

void replicated_log::LogLeader::GuardedLeaderData::checkCommitIndex(std::weak_ptr<LogLeader> const& parentLog) {
  auto const quorum_size = _writeConcern;

  // TODO make this so that we can place any predicate here
  std::vector<std::pair<LogIndex, ParticipantId>> indexes;
  std::transform(_follower.begin(), _follower.end(),
                 std::back_inserter(indexes), [](Follower const& f) {
                   return std::make_pair(f.lastAckedIndex, f._impl->getParticipantId());
                 });
  TRI_ASSERT(indexes.size() == _follower.size());

  if (quorum_size <= 0 || quorum_size > indexes.size()) {
    return;
  }

  auto nth = indexes.begin();
  std::advance(nth, quorum_size - 1);

  std::nth_element(indexes.begin(), nth, indexes.end(),
                   [](auto& a, auto& b) { return a.first > b.first; });

  auto& [commitIndex, participant] = indexes.at(quorum_size - 1);
  TRI_ASSERT(commitIndex >= _participant._commitIndex);
  if (commitIndex > _participant._commitIndex) {
    std::vector<ParticipantId> quorum;
    auto last = indexes.begin();
    std::advance(last, quorum_size);
    std::transform(indexes.begin(), last, std::back_inserter(quorum),
                   [](auto& p) { return p.second; });

    auto const quorum_data =
        std::make_shared<QuorumData>(commitIndex, _participant._currentTerm,
                                     std::move(quorum));
    updateCommitIndexLeader(parentLog, commitIndex, quorum_data);
  }
}

auto replicated_log::LogLeader::getReplicatedLogSnapshot() const
    -> immer::flex_vector<LogEntry> {
  auto [log, commitIndex] = _guardedLeaderData.doUnderLock([](auto const& leaderData) {
    return std::make_pair(leaderData._participant._logCore->_log,
                          leaderData._participant._commitIndex);
  });
  return log.take(commitIndex.value);
}

auto replicated_log::LogParticipant::getPersistedLog() const noexcept
    -> std::shared_ptr<PersistedLog> {
  return _logCore->_persistedLog;
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
    auto transientEntries = immer::flex_vector_transient<LogEntry>{};
    std::transform(entriesVp.begin(), entriesVp.end(), std::back_inserter(transientEntries),
                   [](auto const& it) { return LogEntry::fromVelocyPack(it); });
    return std::move(transientEntries).persistent();
  });

  return AppendEntriesRequest{leaderTerm,   leaderId,     prevLogTerm,
                              prevLogIndex, leaderCommit, std::move(entries)};
}

replicated_log::LogCore::LogCore(std::shared_ptr<PersistedLog> persistedLog)
    : _persistedLog(std::move(persistedLog)) {
  if (ADB_UNLIKELY(_persistedLog == nullptr)) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
        "When instantiating ReplicatedLog: "
        "persistedLog must not be a nullptr");
  }

  // TODO this is a cheap trick for now. Later we should be aware of the fact
  //      that the log might not start at 1.
  auto iter = _persistedLog->read(LogIndex{0});
  while (auto entry = iter->next()) {
    _log = _log.push_back(std::move(entry).value());
  }
}

auto replicated_log::ReplicatedLog::becomeLeader(ParticipantId const& id, LogTerm term,
                                                 std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                                                 std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  auto logCore = std::move(*_participant).resign();
  auto leader = std::make_shared<LogLeader>(id, std::move(logCore), term, follower, writeConcern);
  _participant = std::static_pointer_cast<LogParticipantI>(leader);
  return leader;
}

auto replicated_log::ReplicatedLog::becomeFollower(ParticipantId const& id,
                                                   LogTerm term, ParticipantId leaderId)
    -> std::shared_ptr<LogFollower> {
  auto logCore = std::move(*_participant).resign();
  auto follower = std::make_shared<LogFollower>(id, std::move(logCore), term,
                                                std::move(leaderId));
  _participant = std::static_pointer_cast<LogParticipantI>(follower);
  return follower;
}

replicated_log::LogLeader::LocalFollower::LocalFollower(replicated_log::LogLeader& self)
    : self(self) {}

const ParticipantId& replicated_log::LogLeader::LocalFollower::getParticipantId() const noexcept {
  return self.getParticipantId();
}

futures::Future<AppendEntriesResult> replicated_log::LogLeader::LocalFollower::appendEntries(
    AppendEntriesRequest request) {
  // TODO
  TRI_ASSERT(false);
}
