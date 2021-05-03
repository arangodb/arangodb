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

auto appendEntries(LogTerm const currentTerm, LogIndex& commitIndex,
                   replicated_log::InMemoryLog& inMemoryLog,
                   replicated_log::LogCore* logCore, AppendEntriesRequest const& req)
    -> arangodb::futures::Future<AppendEntriesResult> {

  if (logCore == nullptr) {
    return AppendEntriesResult(currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED, AppendEntriesErrorReason::LOST_LOG_CORE);
  }
  // TODO does >= suffice here? Maybe we want to do an atomic operation
  // before increasing our term
  if (req.leaderTerm != currentTerm) {
    return AppendEntriesResult{currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED, AppendEntriesErrorReason::WRONG_TERM};
  }
  // TODO This happily modifies all parameters. Can we refactor that to make it a little nicer?

  if (req.prevLogIndex > LogIndex{0}) {
    auto entry = inMemoryLog.getEntryByIndex(req.prevLogIndex);
    if (!entry.has_value() || entry->logTerm() != req.prevLogTerm) {
      return AppendEntriesResult{currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED, AppendEntriesErrorReason::NO_PREV_LOG_MATCH};
    }
  }

  auto res = logCore->_persistedLog->removeBack(req.prevLogIndex + 1);
  if (!res.ok()) {
    abort();  // TODO abort?
  }

  auto iter = ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
      req.entries.begin(), req.entries.end());
  res = logCore->_persistedLog->insert(iter);
  if (!res.ok()) {
    abort();  // TODO abort?
  }

  auto transientLog = inMemoryLog._log.transient();
  transientLog.take(req.prevLogIndex.value);
  transientLog.append(req.entries.transient());
  inMemoryLog._log = std::move(transientLog).persistent();

  if (commitIndex < req.leaderCommit && !inMemoryLog._log.empty()) {
    commitIndex = std::min(req.leaderCommit, inMemoryLog._log.back().logIndex());
    // TODO Apply operations to state machine here?
    //      Trigger log consumer streams.
  }

  return AppendEntriesResult{currentTerm};
}

}  // namespace follower
}  // namespace

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto logFollowerDataGuard = acquireMutex();
  return follower::appendEntries(_currentTerm, logFollowerDataGuard->_commitIndex,
                                 logFollowerDataGuard->_inMemoryLog,
                                 logFollowerDataGuard->_logCore.get(), req);
}

replicated_log::LogLeader::LogLeader(ParticipantId const& id, LogTerm const term,
                                     std::size_t const writeConcern, InMemoryLog inMemoryLog)
    : _participantId(id),
      _currentTerm(term),
      _writeConcern(writeConcern),
      _guardedLeaderData(*this, std::move(inMemoryLog)) {}

auto replicated_log::LogLeader::instantiateFollowers(
    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
    std::shared_ptr<LocalFollower> const& localFollower, LogIndex lastIndex)
    -> std::vector<FollowerInfo> {
  auto initLastIndex =
      lastIndex == LogIndex{0} ? LogIndex{0} : LogIndex{lastIndex.value - 1};
  std::vector<FollowerInfo> follower_vec;
  follower_vec.reserve(follower.size() + 1);
  follower_vec.emplace_back(localFollower, lastIndex);
  std::transform(follower.cbegin(), follower.cend(), std::back_inserter(follower_vec),
                 [&](std::shared_ptr<AbstractFollower> const& impl) -> FollowerInfo {
                   return FollowerInfo{impl, initLastIndex};
                 });
  return follower_vec;
}

void replicated_log::LogLeader::executeAppendEntriesRequests(
    std::vector<std::optional<PreparedAppendEntryRequest>> requests) {
  for (auto& it : requests) {
    if (it.has_value()) {
      it->_follower->_impl->appendEntries(std::move(it->_request))
          .thenFinal([parentLog = it->_parentLog, &follower = *it->_follower,
                      lastIndex = it->_lastIndex, currentCommitIndex = it->_currentCommitIndex,
                      currentTerm = it->_currentTerm](futures::Try<AppendEntriesResult>&& res) {
            // TODO This has to be noexcept
            if (auto self = parentLog.lock()) {
              auto preparedRequests = std::invoke([&] {
                auto guarded = self->acquireMutex();
                return guarded->handleAppendEntriesResponse(parentLog, follower, lastIndex,
                                                            currentCommitIndex, currentTerm,
                                                            std::move(res));
              });
              executeAppendEntriesRequests(std::move(preparedRequests));
            }
          });
    }
  }
}

auto replicated_log::LogLeader::construct(
    ParticipantId const& id, std::unique_ptr<LogCore> logCore, LogTerm term,
    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
    std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  // Workaround to be able to use make_shared, while LogLeader's constructor
  // is actually protected.
  struct MakeSharedLogLeader : LogLeader {
   public:
    MakeSharedLogLeader(ParticipantId const& id, LogTerm const& term,
                        size_t writeConcern, InMemoryLog inMemoryLog)
        : LogLeader(id, term, writeConcern, std::move(inMemoryLog)) {}
  };

  // TODO this is a cheap trick for now. Later we should be aware of the fact
  //      that the log might not start at 1.
  auto iter = logCore->_persistedLog->read(LogIndex{0});
  auto log = InMemoryLog{};
  while (auto entry = iter->next()) {
    log._log = log._log.push_back(std::move(entry).value());
  }
  auto const lastIndex = log.getLastIndex();

  auto leader = std::make_shared<MakeSharedLogLeader>(id, term, writeConcern, log);
  auto localFollower = std::make_shared<LocalFollower>(*leader, std::move(logCore));

  auto leaderDataGuard = leader->acquireMutex();

  leaderDataGuard->_follower = instantiateFollowers(follower, localFollower, lastIndex);

  leader->_localFollower = std::move(localFollower);

  TRI_ASSERT(leaderDataGuard->_follower.size() >= writeConcern);

  return leader;
}

auto replicated_log::LogLeader::acquireMutex() -> LogLeader::Guard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::acquireMutex() const -> LogLeader::ConstGuard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::resign() && -> std::unique_ptr<LogCore> {
  // TODO Do we need to do more than that, like make sure to refuse future
  //      requests?
  return _guardedLeaderData.doUnderLock([&localFollower = *_localFollower](GuardedLeaderData& leaderData) {
    for (auto& [idx, promise] : leaderData._waitForQueue) {
      promise.setException(basics::Exception(TRI_ERROR_REPLICATION_LEADER_CHANGE,
                                             __FILE__, __LINE__));
    }
    return std::move(localFollower).resign();
  });
}

auto replicated_log::LogLeader::readReplicatedEntryByIndex(LogIndex idx) const
    -> std::optional<LogEntry> {
  return _guardedLeaderData.doUnderLock([&idx](auto& leaderData) -> std::optional<LogEntry> {
    if (auto entry = leaderData._inMemoryLog.getEntryByIndex(idx);
        entry.has_value() && entry->logIndex() <= leaderData._commitIndex) {
      return entry;
    } else {
      return std::nullopt;
    }
  });
}

auto replicated_log::LogFollower::acquireMutex() -> replicated_log::LogFollower::Guard {
  return _guardedFollowerData.getLockedGuard();
}

auto replicated_log::LogFollower::acquireMutex() const
    -> replicated_log::LogFollower::ConstGuard {
  return _guardedFollowerData.getLockedGuard();
}

auto replicated_log::LogFollower::getStatus() const -> LogStatus {
  return _guardedFollowerData.doUnderLock(
      [term = _currentTerm, &leaderId = _leaderId](auto const& followerData) {
        FollowerStatus status;
        status.local = followerData.getLocalStatistics();
        status.leader = leaderId;
        status.term = term;
        return LogStatus{std::move(status)};
      });
}

auto replicated_log::LogFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return _participantId;
}

auto replicated_log::LogFollower::resign() && -> std::unique_ptr<LogCore> {
  return _guardedFollowerData.doUnderLock(
      [](auto& followerData) { return std::move(followerData._logCore); });
}

replicated_log::LogFollower::LogFollower(ParticipantId const& id,
                                         std::unique_ptr<LogCore> logCore,
                                         LogTerm term, ParticipantId leaderId,
                                         replicated_log::InMemoryLog inMemoryLog)
    : _participantId(id),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)) {}

auto replicated_log::LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

replicated_log::LogUnconfiguredParticipant::LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore)
    : _logCore(std::move(logCore)) {}

auto replicated_log::LogUnconfiguredParticipant::resign() && -> std::unique_ptr<LogCore> {
  return std::move(_logCore);
}

auto replicated_log::LogLeader::getStatus() const -> LogStatus {
  return _guardedLeaderData.doUnderLock([term = _currentTerm](auto& leaderData) {
    LeaderStatus status;
    status.local = leaderData.getLocalStatistics();
    status.term = term;
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
  auto const index = _inMemoryLog.getNextIndex();
  _inMemoryLog._log = _inMemoryLog._log.push_back(
      LogEntry{_self._currentTerm, index, std::move(payload)});
  return index;
}

auto replicated_log::LogLeader::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  auto self = acquireMutex();
  return self->waitFor(index);
}

auto replicated_log::LogLeader::GuardedLeaderData::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  if (_commitIndex >= index) {
    return futures::Future<std::shared_ptr<QuorumData>>{std::in_place, _lastQuorum};
  }
  auto it = _waitForQueue.emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

auto replicated_log::LogLeader::getParticipantId() const noexcept -> ParticipantId const& {
  return _participantId;
}

auto replicated_log::LogLeader::runAsyncStep() -> void {
  auto preparedRequests =
      _guardedLeaderData.doUnderLock([self = weak_from_this()](auto& leaderData) {
        return leaderData.prepareAppendEntries(self);
      });
  executeAppendEntriesRequests(std::move(preparedRequests));
}

auto replicated_log::InMemoryLog::getLastIndex() const -> LogIndex {
  return LogIndex{_log.size()};
}

auto replicated_log::InMemoryLog::getNextIndex() const -> LogIndex {
  return LogIndex{_log.size() + 1};
}

auto replicated_log::InMemoryLog::getEntryByIndex(LogIndex const idx) const
    -> std::optional<LogEntry> {
  if (_log.size() < idx.value || idx.value == 0) {
    return std::nullopt;
  }

  auto const& e = _log.at(idx.value - 1);
  TRI_ASSERT(e.logIndex() == idx);
  return e;
}

void replicated_log::LogLeader::GuardedLeaderData::updateCommitIndexLeader(
    std::weak_ptr<LogLeader> const& parentLog, LogIndex newIndex,
    std::shared_ptr<QuorumData> const& quorum) {
  TRI_ASSERT(_commitIndex < newIndex);
  _commitIndex = newIndex;
  _lastQuorum = quorum;
  auto const end = _waitForQueue.upper_bound(_commitIndex);
  for (auto it = _waitForQueue.begin(); it != end; it = _waitForQueue.erase(it)) {
    it->second.setValue(quorum);
  }
}

auto replicated_log::LogLeader::GuardedLeaderData::prepareAppendEntries(std::weak_ptr<LogLeader> const& parentLog)
    -> std::vector<std::optional<PreparedAppendEntryRequest>> {
  auto appendEntryRequests = std::vector<std::optional<PreparedAppendEntryRequest>>{};
  appendEntryRequests.reserve(_follower.size());
  std::transform(_follower.begin(), _follower.end(), std::back_inserter(appendEntryRequests),
                 [this, &parentLog](auto& follower) {
                   return prepareAppendEntry(parentLog, follower);
                 });
  return appendEntryRequests;
}

auto replicated_log::LogLeader::GuardedLeaderData::prepareAppendEntry(
    std::weak_ptr<LogLeader> const& parentLog, FollowerInfo& follower)
    -> std::optional<PreparedAppendEntryRequest> {
  if (follower.requestInFlight) {
    return std::nullopt;  // wait for the request to return
  }

  auto currentCommitIndex = _commitIndex;
  auto lastIndex = _inMemoryLog.getLastIndex();
  if (follower.lastAckedIndex == lastIndex && _commitIndex == follower.lastAckedCommitIndex) {
    return std::nullopt;  // nothingto replicate
  }

  auto const lastAcked = _inMemoryLog.getEntryByIndex(follower.lastAckedIndex);

  AppendEntriesRequest req;
  req.leaderCommit = _commitIndex;
  req.leaderTerm = _self._currentTerm;
  req.leaderId = _self._participantId;

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
  auto preparedRequest = PreparedAppendEntryRequest{};
  preparedRequest._follower = &follower;
  preparedRequest._currentTerm = _self._currentTerm;
  preparedRequest._currentCommitIndex = currentCommitIndex;
  preparedRequest._lastIndex = lastIndex;
  preparedRequest._parentLog = parentLog;
  preparedRequest._request = std::move(req);
  return preparedRequest;
}

auto replicated_log::LogLeader::GuardedLeaderData::handleAppendEntriesResponse(
    std::weak_ptr<LogLeader> const& parentLog, FollowerInfo& follower,
    LogIndex lastIndex, LogIndex currentCommitIndex, LogTerm currentTerm,
    futures::Try<AppendEntriesResult>&& res)
    -> std::vector<std::optional<PreparedAppendEntryRequest>> {
  if (currentTerm != _self._currentTerm) {
    return {};
  }
  follower.requestInFlight = false;
  if (res.hasValue()) {
    follower.numErrorsSinceLastAnswer = 0;
    auto& response = res.get();
    if (response.isSuccess()) {
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
  return prepareAppendEntries(parentLog);
}

auto replicated_log::LogLeader::GuardedLeaderData::getLogIterator(LogIndex fromIdx) const
    -> std::shared_ptr<LogIterator> {
  auto from = _inMemoryLog._log.begin();
  auto const endIdx = _inMemoryLog.getNextIndex();
  TRI_ASSERT(fromIdx < endIdx);
  std::advance(from, fromIdx.value);
  auto to = _inMemoryLog._log.end();
  return std::make_shared<ReplicatedLogIterator>(from, to);
}

void replicated_log::LogLeader::GuardedLeaderData::checkCommitIndex(std::weak_ptr<LogLeader> const& parentLog) {
  auto const quorum_size = _self._writeConcern;

  // TODO make this so that we can place any predicate here
  std::vector<std::pair<LogIndex, ParticipantId>> indexes;
  std::transform(_follower.begin(), _follower.end(),
                 std::back_inserter(indexes), [](FollowerInfo const& f) {
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
  TRI_ASSERT(commitIndex >= _commitIndex);
  if (commitIndex > _commitIndex) {
    std::vector<ParticipantId> quorum;
    auto last = indexes.begin();
    std::advance(last, quorum_size);
    std::transform(indexes.begin(), last, std::back_inserter(quorum),
                   [](auto& p) { return p.second; });

    auto const quorum_data =
        std::make_shared<QuorumData>(commitIndex, _self._currentTerm, std::move(quorum));
    updateCommitIndexLeader(parentLog, commitIndex, quorum_data);
  }
}

auto replicated_log::LogLeader::GuardedLeaderData::getLocalStatistics() const -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = _inMemoryLog.getLastIndex();
  return result;
}

auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics() const
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = _inMemoryLog.getLastIndex();
  return result;
}

replicated_log::LogLeader::GuardedLeaderData::GuardedLeaderData(replicated_log::LogLeader const& self,
                                                                InMemoryLog inMemoryLog)
    : _self(self), _inMemoryLog(std::move(inMemoryLog)) {}

auto replicated_log::LogLeader::getReplicatedLogSnapshot() const
    -> immer::flex_vector<LogEntry> {
  auto [log, commitIndex] = _guardedLeaderData.doUnderLock([](auto const& leaderData) {
    return std::make_pair(leaderData._inMemoryLog._log, leaderData._commitIndex);
  });
  return log.take(commitIndex.value);
}

QuorumData::QuorumData(const LogIndex& index, LogTerm term, std::vector<ParticipantId> quorum)
    : index(index), term(term), quorum(std::move(quorum)) {}

void AppendEntriesResult::toVelocyPack(velocypack::Builder& builder) const {
  {
    velocypack::ObjectBuilder ob(&builder);
    builder.add("term", VPackValue(logTerm.value));
    builder.add("errorCode", VPackValue(errorCode));
    builder.add("reason", VPackValue(int(reason)));
  }
}

auto AppendEntriesResult::fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult {
  auto logTerm = LogTerm{slice.get("term").extract<size_t>()};
  auto errorCode = ErrorCode{slice.get("errorCode").extract<int>()};
  auto reason = AppendEntriesErrorReason{slice.get("reason").extract<int>()};

  TRI_ASSERT(errorCode == TRI_ERROR_NO_ERROR || reason != AppendEntriesErrorReason::NONE);
  return AppendEntriesResult{logTerm, errorCode, reason};
}

AppendEntriesResult::AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode, AppendEntriesErrorReason reason)
    : logTerm(logTerm), errorCode(errorCode), reason(reason) {
  TRI_ASSERT(errorCode == TRI_ERROR_NO_ERROR || reason != AppendEntriesErrorReason::NONE);
}
AppendEntriesResult::AppendEntriesResult(LogTerm logTerm)
    : AppendEntriesResult(logTerm, TRI_ERROR_NO_ERROR, AppendEntriesErrorReason::NONE) {}

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
}

auto replicated_log::ReplicatedLog::becomeLeader(
    ParticipantId const& id, LogTerm term,
    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
    std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  auto logCore = std::move(*_participant).resign();
  auto leader = LogLeader::construct(id, std::move(logCore), term, follower, writeConcern);
  _participant = std::static_pointer_cast<LogParticipantI>(leader);
  return leader;
}

auto replicated_log::ReplicatedLog::becomeFollower(ParticipantId const& id,
                                                   LogTerm term, ParticipantId leaderId)
    -> std::shared_ptr<LogFollower> {
  auto logCore = std::move(*_participant).resign();
  // TODO this is a cheap trick for now. Later we should be aware of the fact
  //      that the log might not start at 1.
  auto iter = logCore->_persistedLog->read(LogIndex{0});
  auto log = InMemoryLog{};
  while (auto entry = iter->next()) {
    log._log = log._log.push_back(std::move(entry).value());
  }
  auto follower = std::make_shared<LogFollower>(id, std::move(logCore), term,
                                                std::move(leaderId), log);
  _participant = std::static_pointer_cast<LogParticipantI>(follower);
  return follower;
}

replicated_log::LogLeader::LocalFollower::LocalFollower(replicated_log::LogLeader& self,
                                                        std::unique_ptr<LogCore> logCore)
    : _self(self), _guardedLogCore(std::move(logCore)) {}

auto replicated_log::LogLeader::LocalFollower::getParticipantId() const noexcept
    -> ParticipantId const& {
  return _self.getParticipantId();
}

auto replicated_log::LogLeader::LocalFollower::appendEntries(AppendEntriesRequest const request)
    -> futures::Future<AppendEntriesResult> {
  auto logCoreGuard = _guardedLogCore.getLockedGuard();
  auto& logCore = logCoreGuard.get();

  if (logCore == nullptr) {
    return AppendEntriesResult{request.leaderTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED, AppendEntriesErrorReason::LOST_LOG_CORE};
  }

  // TODO The LogCore should know its last log index, and we should assert here
  //      that the AppendEntriesRequest matches it.
  auto iter = ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
      request.entries.begin(), request.entries.end());
  if (auto const res = logCore->_persistedLog->insert(iter); !res.ok()) {
    abort(); // TODO abort
  }

  return AppendEntriesResult{request.leaderTerm};
}

auto replicated_log::LogLeader::LocalFollower::resign() && -> std::unique_ptr<LogCore> {
  return _guardedLogCore.doUnderLock([](auto& guardedLogCore){
    auto logCore = std::move(guardedLogCore);
    return logCore;
  });
}

replicated_log::LogFollower::GuardedFollowerData::GuardedFollowerData(
    LogFollower const& self, std::unique_ptr<LogCore> logCore, InMemoryLog inMemoryLog)
    : _self(self), _inMemoryLog(std::move(inMemoryLog)), _logCore(std::move(logCore)) {}
