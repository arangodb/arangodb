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

#include <Basics/StringUtils.h>
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

  ContainerIterator(I begin, I end)
      : _current(std::move(begin)), _end(std::move(end)) {}

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
  explicit ReplicatedLogIterator(immer::flex_vector<LogEntry> container)
      : _container(std::move(container)),
        _begin(_container.begin()),
        _end(_container.end()) {}

  auto next() -> std::optional<LogEntry> override {
    if (_begin != _end) {
      auto const& res = *_begin;
      ++_begin;
      return res;
    }
    return std::nullopt;
  }

 private:
  immer::flex_vector<LogEntry> _container;
  immer::flex_vector<LogEntry>::const_iterator _begin;
  immer::flex_vector<LogEntry>::const_iterator _end;
};

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto [result, toBeResolved] = _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData& self) -> std::pair<AppendEntriesResult, WaitForQueue> {
        if (self._logCore == nullptr) {
          return std::make_pair(AppendEntriesResult(_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::LOST_LOG_CORE),
                                WaitForQueue{});
        }

        if (req.leaderId != _leaderId) {
          return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::INVALID_LEADER_ID},
                                WaitForQueue{});
        }

        // TODO does >= suffice here? Maybe we want to do an atomic operation
        //      before increasing our term
        if (req.leaderTerm != _currentTerm) {
          return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::WRONG_TERM},
                                WaitForQueue{});
        }
        // TODO This happily modifies all parameters. Can we refactor that to make it a little nicer?

        if (req.prevLogIndex > LogIndex{0}) {
          auto entry = self._inMemoryLog.getEntryByIndex(req.prevLogIndex);
          if (!entry.has_value() || entry->logTerm() != req.prevLogTerm) {
            return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                      AppendEntriesErrorReason::NO_PREV_LOG_MATCH},
                                  WaitForQueue{});
          }
        }

        auto res = self._logCore->_persistedLog->removeBack(req.prevLogIndex + 1);
        if (!res.ok()) {
          abort();  // TODO abort?
        }

        auto iter = ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
            req.entries.begin(), req.entries.end());
        res = self._logCore->_persistedLog->insert(iter);
        if (!res.ok()) {
          abort();  // TODO abort?
        }

        auto transientLog = self._inMemoryLog._log.transient();
        transientLog.take(req.prevLogIndex.value);
        transientLog.append(req.entries.transient());
        self._inMemoryLog._log = std::move(transientLog).persistent();

        WaitForQueue toBeResolved;
        if (self._commitIndex < req.leaderCommit && !self._inMemoryLog._log.empty()) {
          self._commitIndex =
              std::min(req.leaderCommit, self._inMemoryLog._log.back().logIndex());

          auto const end = self._waitForQueue.upper_bound(self._commitIndex);
          for (auto it = self._waitForQueue.begin(); it != end;) {
            toBeResolved.insert(self._waitForQueue.extract(it++));
          }
        }

        return std::make_pair(AppendEntriesResult{_currentTerm}, std::move(toBeResolved));
      });

  for (auto& promise : toBeResolved) {
    // TODO what do we resolve this with? QuorumData is not available on follower
    // TODO execute this in a different context.
    promise.second.setValue(std::shared_ptr<QuorumData>{});
  }

  return std::move(result);
}

replicated_log::LogLeader::LogLeader(ParticipantId id, LogTerm const term,
                                     std::size_t const writeConcern, InMemoryLog inMemoryLog)
    : _participantId(std::move(id)),
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
              auto [preparedRequests, resolvedPromises] = std::invoke([&] {
                auto guarded = self->acquireMutex();
                if (guarded->_didResign) {
                  // Is throwing the right thing to do here?
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
                }
                return guarded->handleAppendEntriesResponse(parentLog, follower, lastIndex,
                                                            currentCommitIndex, currentTerm,
                                                            std::move(res));
              });

              // TODO execute this in a different context
              for (auto& promise : resolvedPromises._set) {
                promise.second.setValue(resolvedPromises._quorum);
              }

              executeAppendEntriesRequests(std::move(preparedRequests));
            }
          });
    }
  }
}

auto replicated_log::LogLeader::construct(
    ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
    std::vector<std::shared_ptr<AbstractFollower>> const& followers,
    std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  if (ADB_UNLIKELY(logCore == nullptr)) {
    auto followerIds = std::vector<std::string>{};
    std::transform(followers.begin(), followers.end(), std::back_inserter(followerIds),
                   [](auto const& follower) -> std::string {
                     return follower->getParticipantId();
                   });
    auto message = basics::StringUtils::concatT(
        "LogCore missing when constructing LogLeader, leader id: ", id,
        "term: ", term.value, "writeConcern: ", writeConcern,
        "followers: ", basics::StringUtils::join(followerIds, ", "));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(message));
  }

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

  leaderDataGuard->_follower = instantiateFollowers(followers, localFollower, lastIndex);
  leader->_localFollower = std::move(localFollower);

  // TODO hack
  if (writeConcern <= 1) {
    leaderDataGuard->_commitIndex = leaderDataGuard->_inMemoryLog.getLastIndex();
  }

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
  auto [core, promises] = _guardedLeaderData.doUnderLock(
      [&localFollower = *_localFollower,
       &participantId = _participantId](GuardedLeaderData& leaderData) {
        if (leaderData._didResign) {
          LOG_TOPIC("5d3b8", ERR, Logger::REPLICATION2)
              << "Leader " << participantId << " already resigned!";
          TRI_ASSERT(false);
        }
        leaderData._didResign = true;
        auto queue = std::move(leaderData._waitForQueue);
        leaderData._waitForQueue.clear();
        return std::make_pair(std::move(localFollower).resign(), std::move(queue));
      });

  for (auto& [idx, promise] : promises) {
    promise.setException(basics::Exception(TRI_ERROR_REPLICATION_LEADER_CHANGE,
                                           __FILE__, __LINE__));
  }
  return std::move(core);
}

auto replicated_log::LogLeader::readReplicatedEntryByIndex(LogIndex idx) const
    -> std::optional<LogEntry> {
  return _guardedLeaderData.doUnderLock([&idx](auto& leaderData) -> std::optional<LogEntry> {
    if (leaderData._didResign) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
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
        if (followerData._logCore == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
        }
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

replicated_log::LogFollower::LogFollower(ParticipantId id, std::unique_ptr<LogCore> logCore,
                                         LogTerm term, ParticipantId leaderId,
                                         replicated_log::InMemoryLog inMemoryLog)
    : _participantId(std::move(id)),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)) {}

auto replicated_log::LogFollower::waitFor(LogIndex idx)
    -> replicated_log::LogParticipantI::WaitForFuture {
  auto self = acquireMutex();
  return self->waitFor(idx);
}

auto replicated_log::LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

replicated_log::LogUnconfiguredParticipant::LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore)
    : _logCore(std::move(logCore)) {}

auto replicated_log::LogUnconfiguredParticipant::resign() && -> std::unique_ptr<LogCore> {
  return std::move(_logCore);
}

auto replicated_log::LogUnconfiguredParticipant::waitFor(LogIndex)
    -> replicated_log::LogParticipantI::WaitForFuture {
  // TODO how to resolve a future with an exception?
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto replicated_log::LogLeader::getStatus() const -> LogStatus {
  return _guardedLeaderData.doUnderLock([term = _currentTerm](auto& leaderData) {
    if (leaderData._didResign) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
    LeaderStatus status;
    status.local = leaderData.getLocalStatistics();
    status.term = term;
    for (auto const& f : leaderData._follower) {
      status.follower[f._impl->getParticipantId()] = {LogStatistics{f.lastAckedIndex, f.lastAckedCommitIndex},
                                                      f.lastErrorReason};
    }
    return LogStatus{std::move(status)};
  });
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto replicated_log::LogLeader::insert(LogPayload payload) -> LogIndex {
  // TODO this has to be lock free
  // TODO investigate what order between insert-increaseTerm is required?
  // Currently we use a mutex. Is this the only valid semantic?
  return _guardedLeaderData.doUnderLock([self = this, &payload](auto& leaderData) {
    if (leaderData._didResign) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
    auto const index = leaderData._inMemoryLog.getNextIndex();
    leaderData._inMemoryLog._log = leaderData._inMemoryLog._log.push_back(
        LogEntry{self->_currentTerm, index, std::move(payload)});
    return index;
  });
}

auto replicated_log::LogLeader::waitFor(LogIndex index)
    -> futures::Future<std::shared_ptr<QuorumData>> {
  return _guardedLeaderData.doUnderLock([index](auto& leaderData) {
    if (leaderData._didResign) {
      auto promise = WaitForPromise{};
      promise.setException(basics::Exception(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
                                             __FILE__, __LINE__));
      return promise.getFuture();
    }
    if (leaderData._commitIndex >= index) {
      return futures::Future<std::shared_ptr<QuorumData>>{std::in_place,
                                                          leaderData._lastQuorum};
    }
    auto it = leaderData._waitForQueue.emplace(index, WaitForPromise{});
    auto& promise = it->second;
    auto&& future = promise.getFuture();
    TRI_ASSERT(future.valid());
    return std::move(future);
  });
}

auto replicated_log::LogLeader::getParticipantId() const noexcept -> ParticipantId const& {
  return _participantId;
}

auto replicated_log::LogLeader::runAsyncStep() -> void {
  auto preparedRequests =
      _guardedLeaderData.doUnderLock([self = weak_from_this()](auto& leaderData) {
        if (leaderData._didResign) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
        }
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

auto replicated_log::LogLeader::GuardedLeaderData::updateCommitIndexLeader(
    std::weak_ptr<LogLeader> const& parentLog, LogIndex newIndex,
    std::shared_ptr<QuorumData> const& quorum) -> ResolvedPromiseSet {
  TRI_ASSERT(_commitIndex < newIndex);
  _commitIndex = newIndex;
  _lastQuorum = quorum;

  WaitForQueue toBeResolved;
  auto const end = _waitForQueue.upper_bound(_commitIndex);
  for (auto it = _waitForQueue.begin(); it != end;) {
    toBeResolved.insert(_waitForQueue.extract(it++));
  }
  return ResolvedPromiseSet{std::move(toBeResolved), quorum};
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
    -> std::pair<std::vector<std::optional<PreparedAppendEntryRequest>>, ResolvedPromiseSet> {
  if (currentTerm != _self._currentTerm) {
    return {};
  }
    ResolvedPromiseSet toBeResolved;

  follower.requestInFlight = false;
  if (res.hasValue()) {
    follower.numErrorsSinceLastAnswer = 0;
    auto& response = res.get();
    follower.lastErrorReason = response.reason;
    if (response.isSuccess()) {
      follower.lastAckedIndex = lastIndex;
      follower.lastAckedCommitIndex = currentCommitIndex;
      toBeResolved = checkCommitIndex(parentLog);
    } else {
      // TODO Optimally, we'd like this condition (lastAckedIndex > 0) to be
      //      assertable here. For that to work, we need to make sure that no
      //      other failures than "I don't have that log entry" can lead to this
      //      branch.
      TRI_ASSERT(response.reason != AppendEntriesErrorReason::NONE);
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
  return std::make_pair(prepareAppendEntries(parentLog), std::move(toBeResolved));
}

auto replicated_log::LogLeader::GuardedLeaderData::getLogIterator(LogIndex fromIdx) const
    -> std::unique_ptr<LogIterator> {
  auto const endIdx = _inMemoryLog.getNextIndex();
  TRI_ASSERT(fromIdx < endIdx);
  auto log = _inMemoryLog._log.drop(fromIdx.value);
  return std::make_unique<ReplicatedLogIterator>(std::move(log));
}

auto replicated_log::LogLeader::GuardedLeaderData::checkCommitIndex(
    std::weak_ptr<LogLeader> const& parentLog) -> ResolvedPromiseSet {
  auto const quorum_size = _self._writeConcern;

  // TODO make this so that we can place any predicate here
  std::vector<std::pair<LogIndex, ParticipantId>> indexes;
  std::transform(_follower.begin(), _follower.end(),
                 std::back_inserter(indexes), [](FollowerInfo const& f) {
                   return std::make_pair(f.lastAckedIndex, f._impl->getParticipantId());
                 });
  TRI_ASSERT(indexes.size() == _follower.size());

  if (quorum_size <= 0 || quorum_size > indexes.size()) {
    return {};
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
    return updateCommitIndexLeader(parentLog, commitIndex, quorum_data);
  }
  return {};
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
    if (leaderData._didResign) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
    return std::make_pair(leaderData._inMemoryLog._log, leaderData._commitIndex);
  });
  return log.take(commitIndex.value);
}

auto replicated_log::LogLeader::waitForIterator(LogIndex index)
    -> replicated_log::LogParticipantI::WaitForIteratorFuture {
  TRI_ASSERT(index != LogIndex{0});
  return waitFor(index).thenValue([this, self = shared_from_this(), index](auto&& quorum) {
    return _guardedLeaderData.doUnderLock(
        [&](GuardedLeaderData& leaderData) { return leaderData.getLogIterator(LogIndex{index.value - 1}); });
  });
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

AppendEntriesResult::AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode,
                                         AppendEntriesErrorReason reason)
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
    ParticipantId id, LogTerm term,
    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
    std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  auto leader = std::shared_ptr<LogLeader>{};
  {
    std::unique_lock guard(_mutex);
    // TODO Resign: will resolve some promises because leader resigned
    //      those promises will call ReplicatedLog::getLeader() -> DEADLOCK
    auto logCore = std::move(*_participant).resign();
    leader = LogLeader::construct(std::move(id), std::move(logCore), term, follower, writeConcern);
    _participant = std::static_pointer_cast<LogParticipantI>(leader);
  }

  //resolve(promises);

  return leader;
}

auto replicated_log::ReplicatedLog::becomeFollower(ParticipantId id,
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

auto replicated_log::ReplicatedLog::getParticipant() const
    -> std::shared_ptr<LogParticipantI> {
  std::unique_lock guard(_mutex);
  return _participant;
}

auto replicated_log::ReplicatedLog::getLeader() const -> std::shared_ptr<LogLeader> {
    auto log = getParticipant();
    if (auto leader =
                std::dynamic_pointer_cast<arangodb::replication2::replicated_log::LogLeader>(log);
            log != nullptr) {
        return leader;
    } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
    }
}

auto replicated_log::ReplicatedLog::getFollower() const -> std::shared_ptr<LogFollower> {
    auto log = getParticipant();
    if (auto leader =
                std::dynamic_pointer_cast<arangodb::replication2::replicated_log::LogFollower>(log);
            log != nullptr) {
        return leader;
    } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
    }
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
    return AppendEntriesResult{request.leaderTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                               AppendEntriesErrorReason::LOST_LOG_CORE};
  }

  // TODO The LogCore should know its last log index, and we should assert here
  //      that the AppendEntriesRequest matches it.
  auto iter = ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
      request.entries.begin(), request.entries.end());
  if (auto const res = logCore->_persistedLog->insert(iter); !res.ok()) {
    abort();  // TODO abort
  }

  return AppendEntriesResult{request.leaderTerm};
}

auto replicated_log::LogLeader::LocalFollower::resign() && -> std::unique_ptr<LogCore> {
  return _guardedLogCore.doUnderLock([](auto& guardedLogCore) {
    auto logCore = std::move(guardedLogCore);
    return logCore;
  });
}

replicated_log::LogFollower::GuardedFollowerData::GuardedFollowerData(
    LogFollower const& self, std::unique_ptr<LogCore> logCore, InMemoryLog inMemoryLog)
    : _self(self), _inMemoryLog(std::move(inMemoryLog)), _logCore(std::move(logCore)) {}

auto replicated_log::LogFollower::GuardedFollowerData::waitFor(LogIndex index)
    -> replicated_log::LogParticipantI::WaitForFuture {
  if (_commitIndex >= index) {
    // TODO give current term?
    return futures::Future<std::shared_ptr<QuorumData>>{std::in_place, nullptr};
  }
  auto it = _waitForQueue.emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

std::string arangodb::replication2::to_string(AppendEntriesErrorReason reason) {
  switch (reason) {
    case AppendEntriesErrorReason::NONE:
      return {};
    case AppendEntriesErrorReason::INVALID_LEADER_ID:
      return "leader id was invalid";
    case AppendEntriesErrorReason::LOST_LOG_CORE:
      return "term has changed and an internal state was lost";
    case AppendEntriesErrorReason::WRONG_TERM:
      return "current term is different from leader term";
    case AppendEntriesErrorReason::NO_PREV_LOG_MATCH:
      return "previous log index did not match";
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

void LogStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add("spearHead", VPackValue(spearHead.value));
}

void UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("unconfigured"));
}

void FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("follower"));
  builder.add("leader", VPackValue(leader));
  builder.add("term", VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
}

void LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("leader"));
  builder.add("term", VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, "follower");
    for (auto const& [id, stat] : follower) {
      builder.add(VPackValue(id));
      stat.toVelocyPack(builder);
    }
  }
}

void LeaderStatus::FollowerStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(VPackValue("local"));
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add("spearHead", VPackValue(spearHead.value));
  builder.add("lastErrorReason", VPackValue(int(lastErrorReason)));
  builder.add("lastErrorReasonMessage", VPackValue(to_string(lastErrorReason)));
}

auto replicated_log::LogParticipantI::waitForIterator(LogIndex index)
    -> replicated_log::LogParticipantI::WaitForIteratorFuture {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
