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

#include "LogFollower.h"

#include "Logger/LogContextKeys.h"
#include "Metrics/Gauge.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"

#include <Basics/Exceptions.h>
#include <Basics/Result.h>
#include <Basics/StringUtils.h>
#include <Basics/debugging.h>
#include <Basics/voc-errors.h>
#include <Futures/Promise.h>

#include <Basics/ScopeGuard.h>
#include <Basics/application-exit.h>
#include <algorithm>

#include <utility>
#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto LogFollower::appendEntriesPreFlightChecks(GuardedFollowerData const& data,
                                               AppendEntriesRequest const& req) const noexcept
    -> std::optional<AppendEntriesResult> {
  if (data._logCore == nullptr) {
    LOG_CTX("d290d", DEBUG, _loggerContext)
        << "reject append entries - log core gone";
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              {AppendEntriesErrorReason::ErrorType::kLostLogCore});
  }

  if (data._lastRecvMessageId >= req.messageId) {
    LOG_CTX("d291d", DEBUG, _loggerContext)
        << "reject append entries - message id out dated: " << req.messageId;
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              {AppendEntriesErrorReason::ErrorType::kMessageOutdated});
  }

  if (req.leaderId != _leaderId) {
    LOG_CTX("a2009", DEBUG, _loggerContext)
        << "reject append entries - wrong leader, given = " << req.leaderId
        << " current = " << _leaderId.value_or("<none>");
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              {AppendEntriesErrorReason::ErrorType::kInvalidLeaderId});
  }

  if (req.leaderTerm != _currentTerm) {
    LOG_CTX("dd7a3", DEBUG, _loggerContext)
        << "reject append entries - wrong term, given = " << req.leaderTerm
        << ", current = " << _currentTerm;
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              {AppendEntriesErrorReason::ErrorType::kWrongTerm});
  }

  // It is always allowed to replace the log entirely
  if (req.prevLogEntry.index > LogIndex{0}) {
    if (auto conflict = algorithms::detectConflict(data._inMemoryLog, req.prevLogEntry);
        conflict.has_value()) {
      auto [reason, next] = *conflict;

      LOG_CTX("5971a", DEBUG, _loggerContext)
          << "reject append entries - prev log did not match: " << to_string(reason);
      return AppendEntriesResult::withConflict(_currentTerm, req.messageId, next);
    }
  }

  return std::nullopt;
}

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  MeasureTimeGuard measureTimeGuard{_logMetrics->replicatedLogFollowerAppendEntriesRtUs};

  auto self = _guardedFollowerData.getLockedGuard();

  {
    // Preflight checks - does the leader, log and other stuff match?
    // This code block should not modify the local state, only check values.
    if (auto result = appendEntriesPreFlightChecks(self.get(), req); result.has_value()) {
      return *result;
    }

    self->_lastRecvMessageId = req.messageId;
  }

  {
    // Transactional Code Block
    // This code removes parts of the log and makes sure that
    // disk and in memory always agree. We first create the new state in memory
    // as a copy, then modify the log on disk. This is an atomic operation. If
    // it fails, we forget the new state. Otherwise we replace the old in memory
    // state with the new value.

    if (self->_inMemoryLog.getLastIndex() != req.prevLogEntry.index) {
      auto newInMemoryLog =
          self->_inMemoryLog.takeSnapshotUpToAndIncluding(req.prevLogEntry.index);
      auto res = self->_logCore->removeBack(req.prevLogEntry.index + 1);
      if (!res.ok()) {
        LOG_CTX("f17b8", ERR, _loggerContext)
            << "failed to remove log entries after " << req.prevLogEntry.index;
        return AppendEntriesResult::withPersistenceError(_currentTerm, req.messageId, res);
      }

      // commit the deletion in memory
      static_assert(std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
      self->_inMemoryLog = std::move(newInMemoryLog);
    }
  }

  // If there are no new entries to be appended, we can simply update the commit
  // index and lci and return early.
  auto toBeResolved = std::make_unique<WaitForQueue>();
  if (req.entries.empty()) {
    auto action = self->checkCommitIndex(req.leaderCommit, req.largestCommonIndex,
                                         std::move(toBeResolved));
    auto result = AppendEntriesResult::withOk(self->_follower._currentTerm, req.messageId);
    self.unlock();  // unlock here, action will be executed via destructor
    static_assert(std::is_nothrow_move_constructible_v<AppendEntriesResult>);
    return {std::move(result)};
  }

  // Allocations
  auto newInMemoryLog = std::invoke([&] {
    // if prevLogIndex is 0, we want to replace the entire log
    // Note that req.entries might not start at 1, because the log could be
    // compacted already.
    if (req.prevLogEntry.index == LogIndex{0}) {
      TRI_ASSERT(!req.entries.empty());
      LOG_CTX("14696", DEBUG, _loggerContext)
          << "replacing my log. New logs starts at "
          << req.entries.front().entry().logTermIndexPair() << ".";
      return InMemoryLog{req.entries};
    }
    return self->_inMemoryLog.append(_loggerContext, req.entries);
  });
  auto iter = std::make_unique<InMemoryPersistedLogIterator>(req.entries);

  auto* core = self->_logCore.get();
  static_assert(std::is_nothrow_move_constructible_v<decltype(newInMemoryLog)>);
  auto checkResultAndCommitIndex =
      [selfGuard = std::move(self), req = std::move(req),
       newInMemoryLog = std::move(newInMemoryLog), toBeResolved = std::move(toBeResolved)](
          futures::Try<Result>&& tryRes) mutable -> std::pair<AppendEntriesResult, DeferredAction> {
    // We have to release the guard after this lambda is finished.
    // Otherwise it would be released when the lambda is destroyed, which
    // happens *after* the following thenValue calls have been executed. In
    // particular the lock is held until the end of the future chain is reached.
    // This will cause deadlocks.
    decltype(selfGuard) self = std::move(selfGuard);

    auto const& res = tryRes.get();
    {
      // This code block does not throw any exceptions. This is executed after
      // we wrote to the on-disk-log.
      static_assert(noexcept(res.fail()));
      if (res.fail()) {
        LOG_CTX("216d8", ERR, self->_follower._loggerContext)
            << "failed to insert log entries: " << res.errorMessage();
        return std::make_pair(AppendEntriesResult::withPersistenceError(
                                  self->_follower._currentTerm, req.messageId, res),
                              DeferredAction{});
      }

      // commit the write in memory
      static_assert(std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
      self->_inMemoryLog = std::move(newInMemoryLog);

      LOG_CTX("dd72d", TRACE, self->_follower._loggerContext)
          << "appended " << req.entries.size() << " log entries after "
          << req.prevLogEntry.index << ", leader commit index = " << req.leaderCommit;
    }

    auto action = self->checkCommitIndex(req.leaderCommit, req.largestCommonIndex,
                                         std::move(toBeResolved));

    static_assert(noexcept(
        AppendEntriesResult::withOk(self->_follower._currentTerm, req.messageId)));
    static_assert(std::is_nothrow_move_constructible_v<DeferredAction>);
    return std::make_pair(AppendEntriesResult::withOk(self->_follower._currentTerm, req.messageId),
                          std::move(action));
  };
  static_assert(std::is_nothrow_move_constructible_v<decltype(checkResultAndCommitIndex)>);

  // Action
  return core->insertAsync(std::move(iter), req.waitForSync)
      .then(std::move(checkResultAndCommitIndex))
      .then([measureTime = std::move(measureTimeGuard)](auto&& res) mutable {
        measureTime.fire();
        auto&& [result, action] = res.get();
        // It is okay to fire here, because commitToMemoryAndResolve has
        // released the guard already.
        action.fire();
        return std::move(result);
      });
}

auto replicated_log::LogFollower::GuardedFollowerData::checkCommitIndex(
    LogIndex newCommitIndex, LogIndex newLCI,
    std::unique_ptr<WaitForQueue> outQueue) noexcept -> DeferredAction {
  TRI_ASSERT(outQueue != nullptr) << "expect outQueue to be preallocated";

  auto const generateToBeResolved = [&, this] {
    try {
      auto waitForQueue = _waitForQueue.getLockedGuard();

      auto const end = waitForQueue->upper_bound(_commitIndex);
      for (auto it = waitForQueue->begin(); it != end;) {
        LOG_CTX("69022", TRACE, _follower._loggerContext)
            << "resolving promise for index " << it->first;
        outQueue->insert(waitForQueue->extract(it++));
      }
      return DeferredAction([commitIndex = _commitIndex,
                             toBeResolved = std::move(outQueue)]() noexcept {
        for (auto& it : *toBeResolved) {
          if (!it.second.isFulfilled()) {
            // This only throws if promise was fulfilled earlier.
            it.second.setValue(WaitForResult{commitIndex, std::shared_ptr<QuorumData>{}});
          }
        }
      });
    } catch (std::exception const& e) {
      // If those promises are not fulfilled we can not continue.
      // Note that the move constructor of std::multi_map is not noexcept.
      LOG_CTX("e7a3d", FATAL, this->_follower._loggerContext)
          << "failed to fulfill replication promises due to exception; "
             "system "
             "can not continue. message: "
          << e.what();
      FATAL_ERROR_EXIT();
    } catch (...) {
      // If those promises are not fulfilled we can not continue.
      // Note that the move constructor of std::multi_map is not noexcept.
      LOG_CTX("c0bba", FATAL, _follower._loggerContext)
          << "failed to fulfill replication promises due to exception; "
             "system can not continue";
      FATAL_ERROR_EXIT();
    }
  };

  TRI_ASSERT(newLCI >= _largestCommonIndex)
      << "req.lci = " << newLCI << ", this.lci = " << _largestCommonIndex;
  if (_largestCommonIndex < newLCI) {
    LOG_CTX("fc467", TRACE, _follower._loggerContext)
        << "largest common index went from " << _largestCommonIndex << " to "
        << newLCI << ".";
    _largestCommonIndex = newLCI;
    // TODO do we want to call checkCompaction here?
    std::ignore = checkCompaction();
  }

  if (_commitIndex < newCommitIndex && !_inMemoryLog.empty()) {
    _commitIndex = std::min(newCommitIndex, _inMemoryLog.back().entry().logIndex());
    LOG_CTX("1641d", TRACE, _follower._loggerContext)
        << "increment commit index: " << _commitIndex;
    return generateToBeResolved();
  }

  return {};
}

replicated_log::LogFollower::GuardedFollowerData::GuardedFollowerData(
    LogFollower const& self, std::unique_ptr<LogCore> logCore, InMemoryLog inMemoryLog)
    : _follower(self),
      _inMemoryLog(std::move(inMemoryLog)),
      _logCore(std::move(logCore)) {}

auto replicated_log::LogFollower::getStatus() const -> LogStatus {
  return _guardedFollowerData.doUnderLock([this](auto const& followerData) {
    if (followerData._logCore == nullptr) {
      throw ParticipantResignedException(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
    }
    FollowerStatus status;
    status.local = followerData.getLocalStatistics();
    status.leader = _leaderId;
    status.term = _currentTerm;
    status.largestCommonIndex = followerData._largestCommonIndex;
    return LogStatus{std::move(status)};
  });
}

auto replicated_log::LogFollower::getQuickStatus() const -> QuickLogStatus {
  return _guardedFollowerData.doUnderLock([this](auto const& followerData) {
    if (followerData._logCore == nullptr) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
    }
    constexpr auto kBaseIndex = LogIndex{0};
    return QuickLogStatus{.role = ParticipantRole::kFollower,
                          .term = _currentTerm,
                          .local = followerData.getLocalStatistics(),
                          .leadershipEstablished = followerData._commitIndex > kBaseIndex};
  });
}

auto replicated_log::LogFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return _participantId;
}

auto replicated_log::LogFollower::resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> {
  return _guardedFollowerData.doUnderLock([this](GuardedFollowerData& followerData) {
    LOG_CTX("838fe", DEBUG, _loggerContext) << "follower resign";
    if (followerData._logCore == nullptr) {
      LOG_CTX("55a1d", WARN, _loggerContext)
          << "follower log core is already gone. Resign was called twice!";
      basics::abortOrThrowException(ParticipantResignedException(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
                                                                 ADB_HERE));
    }

    // use a unique ptr because move constructor for multimaps is not noexcept
    auto queue = std::make_unique<WaitForQueue>(
        std::move(followerData._waitForQueue.getLockedGuard().get()));

    auto action = [queue = std::move(queue)]() noexcept {
      std::for_each(queue->begin(), queue->end(), [](auto& pair) {
        pair.second.setException(ParticipantResignedException(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
                                                              ADB_HERE));
      });
    };
    using action_type = decltype(action);

    static_assert(std::is_nothrow_move_constructible_v<action_type>);
    static_assert(std::is_nothrow_constructible_v<DeferredAction, std::add_rvalue_reference_t<action_type>>);

    // make_tuple is noexcept, _logCore is a unique_ptr which is nothrow move constructable
    return std::make_tuple(std::move(followerData._logCore),
                           DeferredAction{std::move(action)});
  });
}

replicated_log::LogFollower::LogFollower(LoggerContext const& logContext,
                                         std::shared_ptr<ReplicatedLogMetrics> logMetrics,
                                         ParticipantId id, std::unique_ptr<LogCore> logCore,
                                         LogTerm term, std::optional<ParticipantId> leaderId,
                                         replicated_log::InMemoryLog inMemoryLog)
    : _logMetrics(std::move(logMetrics)),
      _loggerContext(
          logContext.with<logContextKeyLogComponent>("follower")
              .with<logContextKeyLeaderId>(leaderId.value_or("<none>"))
              .with<logContextKeyTerm>(term)),
      _participantId(std::move(id)),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)) {
  _logMetrics->replicatedLogFollowerNumber->fetch_add(1);
}

auto replicated_log::LogFollower::waitFor(LogIndex idx)
    -> replicated_log::ILogParticipant::WaitForFuture {
  auto self = _guardedFollowerData.getLockedGuard();
  if (self->_commitIndex >= idx) {
    return futures::Future<WaitForResult>{std::in_place, self->_commitIndex,
                                          std::make_shared<QuorumData>(idx, _currentTerm)};
  }
  // emplace might throw a std::bad_alloc but the remainder is noexcept
  // so either you inserted it and or nothing happens
  auto it = self->_waitForQueue.getLockedGuard()->emplace(idx, WaitForPromise{});
  auto& promise = it->second;
  auto future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return future;
}

auto replicated_log::LogFollower::waitForIterator(LogIndex index)
    -> replicated_log::ILogParticipant::WaitForIteratorFuture {
  if (index == LogIndex{0}) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid parameter; log index 0 is invalid");
  }
  return waitFor(index).thenValue([this, self = shared_from_this(),
                                   index](auto&& quorum) -> WaitForIteratorFuture {
    auto [fromIndex, iter] = _guardedFollowerData.doUnderLock(
        [&](GuardedFollowerData& followerData) -> std::pair<LogIndex, std::unique_ptr<LogRangeIterator>> {
          TRI_ASSERT(index <= followerData._commitIndex);

          /*
           * This code here ensures that if only private log entries are present
           * we do not reply with an empty iterator but instead wait for the
           * next entry containing payload.
           */

          auto actualIndex = index;
          while (actualIndex <= followerData._commitIndex) {
            auto memtry = followerData._inMemoryLog.getEntryByIndex(actualIndex);
            if (!memtry.has_value()) {
              break;
            }
            if (memtry->entry().logPayload().has_value()) {
              break;
            }
            actualIndex = actualIndex + 1;
          }

          if (actualIndex > followerData._commitIndex) {
            return std::make_pair(actualIndex, nullptr);
          }

          return std::make_pair(actualIndex, followerData.getCommittedLogIterator(actualIndex));
        });

    // call here, otherwise we deadlock with waitFor
    if (iter == nullptr) {
      return waitForIterator(fromIndex);
    }

    return std::move(iter);
  });
}

auto replicated_log::LogFollower::getLogIterator(LogIndex firstIndex) const
    -> std::unique_ptr<LogIterator> {
  return _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData const& data) -> std::unique_ptr<LogIterator> {
        auto const endIdx = data._inMemoryLog.getLastTermIndexPair().index + 1;
        TRI_ASSERT(firstIndex <= endIdx);
        return data._inMemoryLog.getIteratorFrom(firstIndex);
      });
}

auto replicated_log::LogFollower::getCommittedLogIterator(LogIndex firstIndex) const
    -> std::unique_ptr<LogIterator> {
  return _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData const& data) -> std::unique_ptr<LogIterator> {
        return data.getCommittedLogIterator(firstIndex);
      });
}

auto replicated_log::LogFollower::GuardedFollowerData::getCommittedLogIterator(LogIndex firstIndex) const
    -> std::unique_ptr<LogRangeIterator> {
  auto const endIdx = _inMemoryLog.getNextIndex();
  TRI_ASSERT(firstIndex < endIdx);
  // return an iterator for the range [firstIndex, _commitIndex + 1)
  return _inMemoryLog.getIteratorRange(firstIndex, _commitIndex + 1);
}

replicated_log::LogFollower::~LogFollower() {
  _logMetrics->replicatedLogFollowerNumber->fetch_sub(1);
  if (auto queueEmpty =
          _guardedFollowerData.getLockedGuard()->_waitForQueue.getLockedGuard()->empty();
      !queueEmpty) {
    TRI_ASSERT(false) << "expected wait-for-queue to be empty";
    LOG_CTX("ce7f8", ERR, _loggerContext)
        << "expected wait-for-queue to be empty";
  }
}

auto LogFollower::release(LogIndex doneWithIdx) -> Result {
  return _guardedFollowerData.doUnderLock([&](GuardedFollowerData& self) -> Result {
    TRI_ASSERT(doneWithIdx <= self._inMemoryLog.getLastIndex());
    if (doneWithIdx <= self._releaseIndex) {
      return {};
    }
    self._releaseIndex = doneWithIdx;
    LOG_CTX("a0c95", TRACE, _loggerContext)
        << "new release index set to " << self._releaseIndex;
    return self.checkCompaction();
  });
}

auto LogFollower::waitForLeaderAcked() -> WaitForFuture {
  return waitFor(LogIndex{1});
}

auto LogFollower::getCommitIndex() const noexcept -> LogIndex {
  return _guardedFollowerData.getLockedGuard()->_commitIndex;
}

auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics() const noexcept
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.firstIndex = _inMemoryLog.getFirstIndex();
  result.spearHead = _inMemoryLog.getLastTermIndexPair();
  return result;
}

auto LogFollower::GuardedFollowerData::checkCompaction() -> Result {
  auto const compactionStop = std::min(_largestCommonIndex, _releaseIndex + 1);
  LOG_CTX("080d5", TRACE, _follower._loggerContext)
      << "compaction index calculated as " << compactionStop;
  if (compactionStop <= _inMemoryLog.getFirstIndex() + 1000) {
    // only do a compaction every 1000 entries
    LOG_CTX("ebb9f", TRACE, _follower._loggerContext)
        << "won't trigger a compaction, not enough entries. First index = "
        << _inMemoryLog.getFirstIndex();
    return {};
  }

  auto newLog = _inMemoryLog.release(compactionStop);
  auto res = _logCore->removeFront(compactionStop).get();
  if (res.ok()) {
    _inMemoryLog = std::move(newLog);
  }
  LOG_CTX("f1028", TRACE, _follower._loggerContext)
      << "compaction result = " << res.errorMessage();
  return res;
}
