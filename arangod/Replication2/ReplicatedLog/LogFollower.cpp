////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"

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
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
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
                                               AppendEntriesRequest const& req)
    const noexcept -> std::optional<AppendEntriesResult> {
  if (data._logCore == nullptr) {
    // Note that a `ReplicatedLog` instance, when destroyed, will resign its
    // participant. This is intentional and has been thoroughly discussed to be
    // the preferable behavior in production, so no LogCore can ever be "lost"
    // but still working in the background. It is expected to be unproblematic,
    // as the ReplicatedLogs are the entries in the central log registry in the
    // vocbase.
    // It is an easy pitfall in the tests, however, as it's easy to drop the
    // shared_ptr to the ReplicatedLog, and keep only the one to the
    // participant. In that case, the participant looses its LogCore, which is
    // hard to find out. Thus we increase the log level for this message to make
    // this more visible.
#ifdef ARANGODB_USE_GOOGLE_TESTS
#define LOST_LOG_CORE_LOGLEVEL INFO
#else
#define LOST_LOG_CORE_LOGLEVEL DEBUG
#endif
    LOG_CTX("d290d", LOST_LOG_CORE_LOGLEVEL, _loggerContext)
        << "reject append entries - log core gone";
    return AppendEntriesResult::withRejection(
        _currentTerm, req.messageId,
        {AppendEntriesErrorReason::ErrorType::kLostLogCore},
        data._snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
  }

  if (data._lastRecvMessageId >= req.messageId) {
    LOG_CTX("d291d", DEBUG, _loggerContext)
        << "reject append entries - message id out dated: " << req.messageId;
    return AppendEntriesResult::withRejection(
        _currentTerm, req.messageId,
        {AppendEntriesErrorReason::ErrorType::kMessageOutdated},
        data._snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
  }

  if (_appendEntriesInFlight) {
    LOG_CTX("92282", DEBUG, _loggerContext)
        << "reject append entries - previous append entry still in flight";
    return AppendEntriesResult::withRejection(
        _currentTerm, req.messageId,
        {AppendEntriesErrorReason::ErrorType::kPrevAppendEntriesInFlight},
        data._snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
  }

  if (req.leaderId != _leaderId) {
    LOG_CTX("a2009", DEBUG, _loggerContext)
        << "reject append entries - wrong leader, given = " << req.leaderId
        << " current = " << _leaderId.value_or("<none>");
    return AppendEntriesResult::withRejection(
        _currentTerm, req.messageId,
        {AppendEntriesErrorReason::ErrorType::kInvalidLeaderId},
        data._snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
  }

  if (req.leaderTerm != _currentTerm) {
    LOG_CTX("dd7a3", DEBUG, _loggerContext)
        << "reject append entries - wrong term, given = " << req.leaderTerm
        << ", current = " << _currentTerm;
    return AppendEntriesResult::withRejection(
        _currentTerm, req.messageId,
        {AppendEntriesErrorReason::ErrorType::kWrongTerm},
        data._snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
  }

  // It is always allowed to replace the log entirely
  if (req.prevLogEntry.index > LogIndex{0}) {
    if (auto conflict =
            algorithms::detectConflict(data._inMemoryLog, req.prevLogEntry);
        conflict.has_value()) {
      auto [reason, next] = *conflict;
      LOG_CTX("5971a", DEBUG, _loggerContext)
          << "reject append entries - prev log did not match: "
          << to_string(reason);
      return AppendEntriesResult::withConflict(
          _currentTerm, req.messageId, next,
          data._snapshotProgress ==
              GuardedFollowerData::SnapshotProgress::kCompleted);
    }
  }

  return std::nullopt;
}

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  MeasureTimeGuard measureTimeGuard{
      _logMetrics->replicatedLogFollowerAppendEntriesRtUs};

  auto dataGuard = _guardedFollowerData.getLockedGuard();

  {
    // Preflight checks - does the leader, log and other stuff match?
    // This code block should not modify the local state, only check values.
    if (auto result = appendEntriesPreFlightChecks(dataGuard.get(), req);
        result.has_value()) {
      return *result;
    }

    dataGuard->_lastRecvMessageId = req.messageId;
  }

  // In case of an exception, this scope guard sets the in flight flag to false.
  // _appendEntriesInFlight is an atomic variable, hence we are allowed to set
  // it without acquiring the mutex.
  //
  // _appendEntriesInFlight is set true, only if the _guardedFollowerData mutex
  // is locked. It is set to false precisely once by the scope guard below.
  // Setting it to false does not require the mutex.
  _appendEntriesInFlight = true;
  auto inFlightScopeGuard =
      ScopeGuard([&flag = _appendEntriesInFlight,
                  &cv = _appendEntriesInFlightCondVar]() noexcept {
        flag = false;
        cv.notify_one();
      });

  bool acquireNewSnapshot = false;
  {
    // Invalidate snapshot status
    if (req.prevLogEntry == TermIndexPair{} &&
        req.entries.front().entry().logIndex() > LogIndex{1}) {
      LOG_CTX("6262d", INFO, _loggerContext)
          << "Log truncated - invalidating snapshot";
      if (auto res = dataGuard->_logCore->updateSnapshotState(
              replicated_state::SnapshotStatus::kUninitialized);
          res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      ADB_PROD_ASSERT(_leaderId.has_value());
      acquireNewSnapshot = true;
    }
  }

  {
    // Transactional Code Block
    // This code removes parts of the log and makes sure that
    // disk and in memory always agree. We first create the new state in memory
    // as a copy, then modify the log on disk. This is an atomic operation. If
    // it fails, we forget the new state. Otherwise we replace the old in memory
    // state with the new value.

    if (dataGuard->_inMemoryLog.getLastIndex() != req.prevLogEntry.index) {
      auto newInMemoryLog =
          dataGuard->_inMemoryLog.takeSnapshotUpToAndIncluding(
              req.prevLogEntry.index);
      auto res = dataGuard->_logCore->removeBack(req.prevLogEntry.index + 1);
      if (!res.ok()) {
        LOG_CTX("f17b8", ERR, _loggerContext)
            << "failed to remove log entries after " << req.prevLogEntry.index;
        return AppendEntriesResult::withPersistenceError(
            _currentTerm, req.messageId, res,
            dataGuard->_snapshotProgress ==
                GuardedFollowerData::SnapshotProgress::kCompleted);
      }

      // commit the deletion in memory
      static_assert(
          std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
      dataGuard->_inMemoryLog = std::move(newInMemoryLog);
    }
  }

  // If there are no new entries to be appended, we can simply update the commit
  // index and lci and return early.
  auto toBeResolved = std::make_unique<WaitForQueue>();
  if (req.entries.empty()) {
    auto action = dataGuard->checkCommitIndex(
        req.leaderCommit, req.lowestIndexToKeep, std::move(toBeResolved));
    auto result = AppendEntriesResult::withOk(
        dataGuard->_follower._currentTerm, req.messageId,
        dataGuard->_snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted);
    dataGuard.unlock();  // unlock here, action must be executed after
    inFlightScopeGuard.fire();
    action.fire();
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
    return dataGuard->_inMemoryLog.append(_loggerContext, req.entries);
  });
  auto iter = std::make_unique<InMemoryPersistedLogIterator>(req.entries);

  auto* core = dataGuard->_logCore.get();
  static_assert(std::is_nothrow_move_constructible_v<decltype(newInMemoryLog)>);

  auto const waitForSync = req.waitForSync;
  auto const prevLogEntry = req.prevLogEntry;
  auto checkResultAndCommitIndex =
      [self = shared_from_this(), inFlightGuard = std::move(inFlightScopeGuard),
       req = std::move(req), newInMemoryLog = std::move(newInMemoryLog),
       toBeResolved =
           std::move(toBeResolved)](futures::Try<Result>&& tryRes) mutable
      -> std::pair<AppendEntriesResult, DeferredAction> {
    // We have to release the guard after this lambda is finished.
    // Otherwise it would be released when the lambda is destroyed, which
    // happens *after* the following thenValue calls have been executed. In
    // particular the lock is held until the end of the future chain is reached.
    // This will cause deadlocks.
    decltype(inFlightGuard) inFlightGuardLocal = std::move(inFlightGuard);
    auto data = self->_guardedFollowerData.getLockedGuard();
    if (data->didResign()) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
    }

    auto const& res = tryRes.get();
    {
      // This code block does not throw any exceptions. This is executed after
      // we wrote to the on-disk-log.
      static_assert(noexcept(res.fail()));
      if (res.fail()) {
        LOG_CTX("216d8", ERR, data->_follower._loggerContext)
            << "failed to insert log entries: " << res.errorMessage();
        return std::make_pair(
            AppendEntriesResult::withPersistenceError(
                data->_follower._currentTerm, req.messageId, res,
                data->_snapshotProgress ==
                    GuardedFollowerData::SnapshotProgress::kCompleted),
            DeferredAction{});
      }

      // commit the write in memory
      static_assert(
          std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
      data->_inMemoryLog = std::move(newInMemoryLog);
      self->_logMetrics->replicatedLogNumberAcceptedEntries->count(
          req.entries.size());
      LOG_CTX("dd72d", TRACE, data->_follower._loggerContext)
          << "appended " << req.entries.size() << " log entries after "
          << req.prevLogEntry.index
          << ", leader commit index = " << req.leaderCommit;
    }

    auto action = data->checkCommitIndex(
        req.leaderCommit, req.lowestIndexToKeep, std::move(toBeResolved));

    static_assert(noexcept(AppendEntriesResult::withOk(
        data->_follower._currentTerm, req.messageId,
        dataGuard->_snapshotProgress ==
            GuardedFollowerData::SnapshotProgress::kCompleted)));
    static_assert(std::is_nothrow_move_constructible_v<DeferredAction>);
    return std::make_pair(
        AppendEntriesResult::withOk(
            data->_follower._currentTerm, req.messageId,
            data->_snapshotProgress ==
                GuardedFollowerData::SnapshotProgress::kCompleted),
        std::move(action));
  };
  static_assert(std::is_nothrow_move_constructible_v<
                decltype(checkResultAndCommitIndex)>);

  // Action
  auto f = core->insertAsync(std::move(iter), waitForSync);
  // Release mutex here, otherwise we might deadlock in
  // checkResultAndCommitIndex if another request arrives before the previous
  // one was processed.
  if (acquireNewSnapshot) {
    dataGuard->_snapshotProgress =
        GuardedFollowerData::SnapshotProgress::kInProgress;
    _stateHandle->acquireSnapshot(*_leaderId, prevLogEntry.index + 1);
  }
  dataGuard.unlock();
  return std::move(f)
      .then(std::move(checkResultAndCommitIndex))
      .then([measureTime = std::move(measureTimeGuard)](auto&& res) mutable {
        measureTime.fire();
        auto&& [result, action] = res.get();
        action.fire();

        return std::move(result);
      });
}

auto replicated_log::LogFollower::GuardedFollowerData::checkCommitIndex(
    LogIndex newCommitIndex, LogIndex newLITK,
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
            it.second.setValue(
                WaitForResult{commitIndex, std::shared_ptr<QuorumData>{}});
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

  if (_lowestIndexToKeep < newLITK) {
    LOG_CTX("fc467", TRACE, _follower._loggerContext)
        << "largest common index went from " << _lowestIndexToKeep << " to "
        << newLITK << ".";
    _lowestIndexToKeep = newLITK;
    // TODO do we want to call checkCompaction here?
    std::ignore = checkCompaction();
  }

  if (_commitIndex < newCommitIndex && !_inMemoryLog.empty()) {
    auto const oldCommitIndex = _commitIndex;
    _commitIndex =
        std::min(newCommitIndex, _inMemoryLog.back().entry().logIndex());

    if (_snapshotProgress == SnapshotProgress::kUninitialized) {
      _snapshotProgress = SnapshotProgress::kInProgress;
      _follower._stateHandle->acquireSnapshot(*_follower._leaderId,
                                              _commitIndex);
    }

    // Only call updateCommitIndex after the snapshot is completed. Otherwise,
    // the state manager can trigger applyEntries calls while the snapshot
    // is still being transferred.
    if (_snapshotProgress == SnapshotProgress::kCompleted) {
      _follower._stateHandle->updateCommitIndex(newCommitIndex);
    }
    _follower._logMetrics->replicatedLogNumberCommittedEntries->count(
        _commitIndex.value - oldCommitIndex.value);
    LOG_CTX("1641d", TRACE, _follower._loggerContext)
        << "increment commit index: " << _commitIndex;
    return generateToBeResolved();
  }

  return {};
}

auto replicated_log::LogFollower::GuardedFollowerData::didResign()
    const noexcept -> bool {
  return _logCore == nullptr;
}

replicated_log::LogFollower::GuardedFollowerData::GuardedFollowerData(
    LogFollower const& self, std::unique_ptr<LogCore> logCore,
    InMemoryLog inMemoryLog)
    : _follower(self),
      _inMemoryLog(std::move(inMemoryLog)),
      _logCore(std::move(logCore)) {}

auto replicated_log::LogFollower::getStatus() const -> LogStatus {
  return _guardedFollowerData.doUnderLock([this](auto const& followerData) {
    if (followerData._logCore == nullptr) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
    }
    FollowerStatus status;
    status.local = followerData.getLocalStatistics();
    status.leader = _leaderId;
    status.term = _currentTerm;
    status.lowestIndexToKeep = followerData._lowestIndexToKeep;
    status.compactionStatus = followerData.compactionStatus;
    status.snapshotAvailable =
        followerData._snapshotProgress ==
        GuardedFollowerData::SnapshotProgress::kCompleted;
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
    return QuickLogStatus{
        .role = ParticipantRole::kFollower,
        .term = _currentTerm,
        .local = followerData.getLocalStatistics(),
        .leadershipEstablished = followerData._commitIndex > kBaseIndex,
        .snapshotAvailable = followerData._snapshotProgress ==
                             GuardedFollowerData::SnapshotProgress::kCompleted};
  });
}

auto LogFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return _participantId;
}

auto LogFollower::resign() && -> std::tuple<std::unique_ptr<LogCore>,
                                            DeferredAction> {
  auto result = _guardedFollowerData.doUnderLock(
      [this](GuardedFollowerData& followerData) {
        LOG_CTX("838fe", DEBUG, _loggerContext) << "follower resign";
        if (followerData.didResign()) {
          LOG_CTX("55a1d", WARN, _loggerContext)
              << "follower log core is already gone. Resign was called twice!";
          basics::abortOrThrowException(ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              ADB_HERE));
        }

        // use a unique ptr because move constructor for multimaps is not
        // noexcept
        struct Queues {
          WaitForQueue waitForQueue;
          WaitForBag waitForResignQueue;
        };
        auto queues = std::make_unique<Queues>();
        std::swap(queues->waitForQueue,
                  followerData._waitForQueue.getLockedGuard().get());
        queues->waitForResignQueue =
            std::move(followerData._waitForResignQueue);

        auto action = [queues = std::move(queues)]() noexcept {
          std::for_each(
              queues->waitForQueue.begin(), queues->waitForQueue.end(),
              [](auto& pair) {
                pair.second.setException(ParticipantResignedException(
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
                    ADB_HERE));
              });
          queues->waitForResignQueue.resolveAll();
        };
        using action_type = decltype(action);

        static_assert(std::is_nothrow_move_constructible_v<action_type>);
        static_assert(
            std::is_nothrow_constructible_v<
                DeferredAction, std::add_rvalue_reference_t<action_type>>);

        // make_tuple is noexcept, _logCore is a unique_ptr which is nothrow
        // move constructable
        return std::make_tuple(std::move(followerData._logCore),
                               DeferredAction{std::move(action)});
      });
  {
    auto methods = _stateHandle->resignCurrentState();
    ADB_PROD_ASSERT(methods != nullptr);
    // We *must not* use this handle any longer. Its ownership is shared with
    // our parent ReplicatedLog, which will pass it as necessary.
    _stateHandle = nullptr;
  }
  return result;
}

replicated_log::LogFollower::LogFollower(
    LoggerContext const& logContext,
    std::shared_ptr<ReplicatedLogMetrics> logMetrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
    std::optional<ParticipantId> leaderId,
    std::shared_ptr<IReplicatedStateHandle> stateHandle,
    replicated_log::InMemoryLog inMemoryLog,
    std::shared_ptr<ILeaderCommunicator> leaderCommunicator)
    : _logMetrics(std::move(logMetrics)),
      _options(std::move(options)),
      _loggerContext(
          logContext.with<logContextKeyLogComponent>("follower")
              .with<logContextKeyLeaderId>(leaderId.value_or("<none>"))
              .with<logContextKeyTerm>(term)),
      _participantId(std::move(id)),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _stateHandle(std::move(stateHandle)),
      leaderCommunicator(std::move(leaderCommunicator)),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)) {
  auto guard = _guardedFollowerData.getLockedGuard();
  auto snapshotStatus = guard->_logCore->getSnapshotState();
  if (snapshotStatus.fail()) {
    THROW_ARANGO_EXCEPTION(snapshotStatus.result());
  }
  if (snapshotStatus == replicated_state::SnapshotStatus::kCompleted) {
    guard->_snapshotProgress =
        GuardedFollowerData::SnapshotProgress::kCompleted;
  } else {
    guard->_snapshotProgress =
        GuardedFollowerData::SnapshotProgress::kUninitialized;
  }

  LOG_CTX("c3791", DEBUG, _loggerContext)
      << "loaded snapshot status: " << to_string(*snapshotStatus);

  struct MethodsImpl : IReplicatedLogFollowerMethods {
    explicit MethodsImpl(LogFollower& log) : _log(log) {}
    auto releaseIndex(LogIndex index) -> void override {
      if (auto res = _log.release(index); res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
    auto getLogSnapshot() -> InMemoryLog override {
      return _log.copyInMemoryLog();
    }
    auto snapshotCompleted() -> Result override {
      return _log.onSnapshotCompleted();
    }
    auto waitFor(LogIndex index) -> WaitForFuture override {
      return _log.waitFor(index);
    }
    auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
      return _log.waitForIterator(index);
    }
    LogFollower& _log;
  };
  LOG_CTX("f3668", DEBUG, _loggerContext)
      << "calling becomeFollower on state handle";
  _stateHandle->becomeFollower(std::make_unique<MethodsImpl>(*this));
  _logMetrics->replicatedLogFollowerNumber->fetch_add(1);
}

auto replicated_log::LogFollower::waitFor(LogIndex idx)
    -> replicated_log::ILogParticipant::WaitForFuture {
  auto self = _guardedFollowerData.getLockedGuard();
  if (self->didResign()) {
    auto promise = WaitForPromise{};
    promise.setException(ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE));
    return promise.getFuture();
  }
  if (self->_commitIndex >= idx) {
    return futures::Future<WaitForResult>{
        std::in_place, self->_commitIndex,
        std::make_shared<QuorumData>(idx, _currentTerm)};
  }
  // emplace might throw a std::bad_alloc but the remainder is noexcept
  // so either you inserted it and or nothing happens
  // TODO locking ok? Iterator stored but lock guard is temporary
  auto it =
      self->_waitForQueue.getLockedGuard()->emplace(idx, WaitForPromise{});
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
  return waitFor(index).thenValue([this, self = shared_from_this(), index](
                                      auto&& quorum) -> WaitForIteratorFuture {
    auto [fromIndex, iter] = _guardedFollowerData.doUnderLock(
        [&](GuardedFollowerData& followerData)
            -> std::pair<LogIndex, std::unique_ptr<LogRangeIterator>> {
          TRI_ASSERT(index <= followerData._commitIndex);

          /*
           * This code here ensures that if only private log entries are present
           * we do not reply with an empty iterator but instead wait for the
           * next entry containing payload.
           */

          auto actualIndex =
              std::max(index, followerData._inMemoryLog.getFirstIndex());
          while (actualIndex <= followerData._commitIndex) {
            auto memtry =
                followerData._inMemoryLog.getEntryByIndex(actualIndex);
            TRI_ASSERT(memtry.has_value())
                << "first index is "
                << followerData._inMemoryLog
                       .getFirstIndex();  // should always have a value
            if (!memtry.has_value()) {
              break;
            }
            if (memtry->entry().hasPayload()) {
              break;
            }
            actualIndex = actualIndex + 1;
          }

          if (actualIndex > followerData._commitIndex) {
            return std::make_pair(actualIndex, nullptr);
          }

          return std::make_pair(
              actualIndex, followerData.getCommittedLogIterator(actualIndex));
        });

    // call here, otherwise we deadlock with waitFor
    if (iter == nullptr) {
      return waitForIterator(fromIndex);
    }

    return std::move(iter);
  });
}

auto replicated_log::LogFollower::GuardedFollowerData::getCommittedLogIterator(
    LogIndex firstIndex) const -> std::unique_ptr<LogRangeIterator> {
  auto const endIdx = _inMemoryLog.getNextIndex();
  TRI_ASSERT(firstIndex < endIdx);
  // return an iterator for the range [firstIndex, _commitIndex + 1)
  return _inMemoryLog.getIteratorRange(firstIndex, _commitIndex + 1);
}

replicated_log::LogFollower::~LogFollower() {
  _logMetrics->replicatedLogFollowerNumber->fetch_sub(1);
  if (auto queueEmpty = _guardedFollowerData.getLockedGuard()
                            ->_waitForQueue.getLockedGuard()
                            ->empty();
      !queueEmpty) {
    TRI_ASSERT(false) << "expected wait-for-queue to be empty";
    LOG_CTX("ce7f8", ERR, _loggerContext)
        << "expected wait-for-queue to be empty";
  }
}

auto LogFollower::release(LogIndex doneWithIdx) -> Result {
  auto guard = _guardedFollowerData.getLockedGuard();
  if (guard->didResign()) {
    return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
  }
  guard.wait(_appendEntriesInFlightCondVar,
             [&] { return !_appendEntriesInFlight; });

  TRI_ASSERT(doneWithIdx <= guard->_inMemoryLog.getLastIndex());
  if (doneWithIdx <= guard->_releaseIndex) {
    return {};
  }
  guard->_releaseIndex = doneWithIdx;
  LOG_CTX("a0c95", TRACE, _loggerContext)
      << "new release index set to " << guard->_releaseIndex;
  return guard->checkCompaction();
}

auto LogFollower::waitForLeaderAcked() -> WaitForFuture {
  return waitFor(LogIndex{1});
}

auto LogFollower::getLeader() const noexcept
    -> std::optional<ParticipantId> const& {
  return _leaderId;
}

auto LogFollower::construct(
    LoggerContext const& loggerContext,
    std::shared_ptr<ReplicatedLogMetrics> logMetrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
    std::optional<ParticipantId> leaderId,
    std::shared_ptr<IReplicatedStateHandle> stateHandle,
    std::shared_ptr<ILeaderCommunicator> leaderCommunicator)
    -> std::shared_ptr<LogFollower> {
  auto log = InMemoryLog::loadFromLogCore(*logCore);

  auto const lastIndex = log.getLastTermIndexPair();

  if (lastIndex.term >= term) {
    LOG_CTX("2d80c", WARN, loggerContext)
        << "Becoming follower in term " << term
        << " but spearhead is already at term " << lastIndex.term;
  }

  struct MakeSharedWrapper : LogFollower {
    MakeSharedWrapper(
        LoggerContext const& loggerContext,
        std::shared_ptr<ReplicatedLogMetrics> logMetrics,
        std::shared_ptr<ReplicatedLogGlobalSettings const> options,
        ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
        std::optional<ParticipantId> leaderId,
        std::shared_ptr<IReplicatedStateHandle> stateHandle,
        InMemoryLog inMemoryLog,
        std::shared_ptr<ILeaderCommunicator> leaderCommunicator)
        : LogFollower(loggerContext, std::move(logMetrics), std::move(options),
                      std::move(id), std::move(logCore), term,
                      std::move(leaderId), std::move(stateHandle),
                      std::move(inMemoryLog), std::move(leaderCommunicator)) {}
  };

  return std::make_shared<MakeSharedWrapper>(
      loggerContext, std::move(logMetrics), std::move(options), std::move(id),
      std::move(logCore), term, std::move(leaderId), std::move(stateHandle),
      std::move(log), std::move(leaderCommunicator));
}

auto LogFollower::copyInMemoryLog() const -> InMemoryLog {
  return _guardedFollowerData.getLockedGuard()->_inMemoryLog;
}

Result LogFollower::onSnapshotCompleted() {
  auto guard = _guardedFollowerData.getLockedGuard();
  auto res = guard->_logCore->updateSnapshotState(
      replicated_state::SnapshotStatus::kCompleted);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  LOG_CTX("80094", DEBUG, _loggerContext)
      << "Snapshot status updated to completed on persistent storage";
  ADB_PROD_ASSERT(guard->_snapshotProgress ==
                  GuardedFollowerData::SnapshotProgress::kInProgress);
  guard->_snapshotProgress = GuardedFollowerData::SnapshotProgress::kCompleted;
  leaderCommunicator->reportSnapshotAvailable(guard->_lastRecvMessageId)
      .thenFinal(
          [self = shared_from_this()](futures::Try<Result>&& res) noexcept {
            auto realRes = basics::catchToResult([&] { return res.get(); });
            if (realRes.fail()) {
              LOG_CTX("9db47", ERR, self->_loggerContext)
                  << "failed to update snapshot status on leader";
              FATAL_ERROR_EXIT();  // TODO this has to be more resilient
            }
            LOG_CTX("600de", DEBUG, self->_loggerContext)
                << "snapshot status updated on leader";
          });
  return {};
}

auto LogFollower::compact() -> ResultT<CompactionResult> {
  auto guard = _guardedFollowerData.getLockedGuard();
  auto const& [stopIndex, reason] = guard->calcCompactionStop();
  LOG_CTX("aed29", INFO, _loggerContext)
      << "starting explicit compaction up to index " << stopIndex << "; "
      << reason;
  return guard->runCompaction(true);
}

auto replicated_log::LogFollower::GuardedFollowerData::calcCompactionStopIndex()
    const noexcept -> LogIndex {
  return std::min(_lowestIndexToKeep, _releaseIndex + 1);
}

auto replicated_log::LogFollower::GuardedFollowerData::calcCompactionStop()
    const noexcept -> std::pair<LogIndex, CompactionStopReason> {
  auto const stopIndex = calcCompactionStopIndex();
  ADB_PROD_ASSERT(stopIndex <= _inMemoryLog.getLastIndex())
      << "stopIndex is " << stopIndex << ", releaseIndex = " << _releaseIndex
      << ", lowestIndexToKeep = " << _lowestIndexToKeep
      << ", last index = " << _inMemoryLog.getLastIndex();
  if (stopIndex == _inMemoryLog.getLastIndex()) {
    return {stopIndex, {CompactionStopReason::NothingToCompact{}}};
  } else if (stopIndex == _releaseIndex + 1) {
    return {stopIndex,
            {CompactionStopReason::NotReleasedByStateMachine{_releaseIndex}}};
  } else if (stopIndex == _lowestIndexToKeep) {
    return {stopIndex, {CompactionStopReason::LeaderBlocksReleaseEntry{}}};
  } else {
    ADB_PROD_ASSERT(false) << "stopIndex is " << stopIndex
                           << " releaseIndex = " << _releaseIndex
                           << " lowestIndexToKeep = " << _lowestIndexToKeep;
  }
  std::abort();  // avoid warnings
}

auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics()
    const noexcept -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.firstIndex = _inMemoryLog.getFirstIndex();
  result.spearHead = _inMemoryLog.getLastTermIndexPair();
  result.releaseIndex = _releaseIndex;
  return result;
}

auto LogFollower::GuardedFollowerData::checkCompaction() -> Result {
  auto const compactionStop = calcCompactionStopIndex();
  LOG_CTX("080d5", TRACE, _follower._loggerContext)
      << "compaction index calculated as " << compactionStop;
  return runCompaction(false).result();
}

auto LogFollower::GuardedFollowerData::runCompaction(bool ignoreThreshold)
    -> ResultT<CompactionResult> {
  auto const nextCompactionAt = _inMemoryLog.getFirstIndex() +
                                _follower._options->_thresholdLogCompaction;
  if (!ignoreThreshold && _inMemoryLog.getLastIndex() <=
                              _inMemoryLog.getFirstIndex() +
                                  _follower._options->_thresholdLogCompaction) {
    // only do a compaction every _thresholdLogCompaction entries
    LOG_CTX("ebb9f", TRACE, _follower._loggerContext)
        << "won't trigger a compaction, not enough entries. First index = "
        << _inMemoryLog.getFirstIndex();
    compactionStatus.stop = CompactionStopReason{
        CompactionStopReason::CompactionThresholdNotReached{nextCompactionAt}};
    return {};
  }

  auto const [compactionStop, reason] = calcCompactionStop();
  ADB_PROD_ASSERT(compactionStop >= _inMemoryLog.getFirstIndex());
  auto const compactionRange =
      LogRange(_inMemoryLog.getFirstIndex(), compactionStop);
  auto const numberOfCompactedEntries = compactionRange.count();
  auto res = Result{};
  if (numberOfCompactedEntries > 0) {
    auto newLog = _inMemoryLog.release(compactionStop);
    res = _logCore->removeFront(compactionStop).get();
    if (res.ok()) {
      _inMemoryLog = std::move(newLog);
      _follower._logMetrics->replicatedLogNumberCompactedEntries->count(
          numberOfCompactedEntries);
      compactionStatus.lastCompaction = CompactionStatus::Compaction{
          .time = CompactionStatus::clock::now(), .range = compactionRange};
    }
    LOG_CTX("f1028", TRACE, _follower._loggerContext)
        << "compaction result = " << res.errorMessage();
  }

  if (res.fail()) {
    LOG_CTX("5b57b", WARN, _follower._loggerContext)
        << "compaction failed: " << res.errorMessage();
    return res;
  } else {
    compactionStatus.stop = reason;
    return CompactionResult{.numEntriesCompacted = numberOfCompactedEntries,
                            .stopReason = reason};
  }
}

auto LogFollower::GuardedFollowerData::waitForResign()
    -> std::pair<futures::Future<futures::Unit>, DeferredAction> {
  if (!didResign()) {
    auto future = _waitForResignQueue.addWaitFor();
    return {std::move(future), DeferredAction{}};
  } else {
    TRI_ASSERT(_waitForResignQueue.empty());
    auto promise = futures::Promise<futures::Unit>{};
    auto future = promise.getFuture();

    auto action =
        DeferredAction([promise = std::move(promise)]() mutable noexcept {
          TRI_ASSERT(promise.valid());
          promise.setValue();
        });

    return {std::move(future), std::move(action)};
  }
}
