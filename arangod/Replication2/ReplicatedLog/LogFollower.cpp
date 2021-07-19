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

#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/LogContextKeys.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "RestServer/Metrics.h"

#include <Basics/Exceptions.h>
#include <Basics/Result.h>
#include <Basics/debugging.h>
#include <Basics/voc-errors.h>
#include <Futures/Promise.h>

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
#include <Basics/ScopeGuard.h>
#include <Basics/application-exit.h>
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
                                              AppendEntriesErrorReason::LOST_LOG_CORE);
  }

  if (data._lastRecvMessageId >= req.messageId) {
    LOG_CTX("d291d", DEBUG, _loggerContext)
        << "reject append entries - message id out dated: " << req.messageId;
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              AppendEntriesErrorReason::MESSAGE_OUTDATED);
  }

  if (req.leaderId != _leaderId) {
    LOG_CTX("a2009", DEBUG, _loggerContext)
        << "reject append entries - wrong leader, given = " << req.leaderId
        << " current = " << _leaderId.value_or("<none>");
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              AppendEntriesErrorReason::INVALID_LEADER_ID);
  }

  if (req.leaderTerm != _currentTerm) {
    LOG_CTX("dd7a3", DEBUG, _loggerContext)
        << "reject append entries - wrong term, given = " << req.leaderTerm
        << ", current = " << _currentTerm;
    return AppendEntriesResult::withRejection(_currentTerm, req.messageId,
                                              AppendEntriesErrorReason::WRONG_TERM);
  }

  // It is always allowed to replace the log entirely
  if (req.prevLogEntry.index > LogIndex{0}) {
    if (auto conflict =
            algorithms::detectConflict(data._inMemoryLog, req.prevLogEntry);
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
    auto newInMemoryLog = self->_inMemoryLog.takeSnapshotUpToAndIncluding(req.prevLogEntry.index);

    if (self->_inMemoryLog.getLastIndex() != req.prevLogEntry.index) {
      auto res = self->_logCore->removeBack(req.prevLogEntry.index + 1);
      if (!res.ok()) {
        LOG_CTX("f17b8", ERR, _loggerContext)
            << "failed to remove log entries after " << req.prevLogEntry.index;
        return AppendEntriesResult::withPersistenceError(_currentTerm, req.messageId, res);
      }
    }

    // commit the deletion in memory
    static_assert(std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
    self->_inMemoryLog = std::move(newInMemoryLog);
  }

  struct WaitForQueueResolve {
    WaitForQueueResolve(Guarded<WaitForQueue>::mutex_guard_type guard, LogIndex commitIndex) noexcept
        : _guard(std::move(guard)),
          begin(_guard->begin()),
          end(_guard->upper_bound(commitIndex)) {}

    Guarded<WaitForQueue>::mutex_guard_type _guard;
    WaitForQueue::iterator begin;
    WaitForQueue::iterator end;
  };

  // Allocations
  auto newInMemoryLog = self->_inMemoryLog.append(_loggerContext, req.entries);
  auto iter = std::make_unique<ReplicatedLogIterator>(req.entries);
  auto toBeResolved = std::make_unique<std::optional<WaitForQueueResolve>>();

  auto* core = self->_logCore.get();
  static_assert(std::is_nothrow_move_constructible_v<decltype(newInMemoryLog)>);
  auto commitToMemoryAndResolve = [maybeSelf = std::optional<decltype(self)>(std::move(self)),
                                   req = std::move(req),
                                   newInMemoryLog = std::move(newInMemoryLog),
                                   toBeResolved = std::move(toBeResolved)](
                                      Result&& res) mutable noexcept {
    // We have to release the guard after this lambda is finished.
    // Otherwise it would be release when the lambda is destructed, which
    // happens *after* the following thenValue calls have been executed. In
    // particular the lock is held until the end of the future chain is reached.
    // This will cause deadlocks.
    TRI_DEFER({ maybeSelf.reset(); });
    TRI_ASSERT(maybeSelf.has_value());
    auto& self = maybeSelf.value();

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

    auto action = std::invoke([&]() noexcept -> DeferredAction {
      if (self->_commitIndex < req.leaderCommit && !self->_inMemoryLog.empty()) {
        self->_commitIndex =
            std::min(req.leaderCommit, self->_inMemoryLog.back().logIndex());
        LOG_CTX("1641d", TRACE, self->_follower._loggerContext)
            << "increment commit index: " << self->_commitIndex;

        *toBeResolved = WaitForQueueResolve{self->_waitForQueue.getLockedGuard(),
                                            self->_commitIndex};
        return DeferredAction([toBeResolved = std::move(toBeResolved)]() noexcept {
          auto& resolve = toBeResolved->value();
          for (auto it = resolve.begin; it != resolve.end; it = resolve._guard->erase(it)) {
            if (!it->second.isFulfilled()) {
              // This only throws if promise was fulfilled earlier.
              it->second.setValue(std::shared_ptr<QuorumData>{});
            }
          }
        });
      }

      return {};
    });

    static_assert(noexcept(AppendEntriesResult::withOk(self->_follower._currentTerm, req.messageId)));
    static_assert(std::is_nothrow_move_constructible_v<DeferredAction>);
    return std::make_pair(AppendEntriesResult::withOk(self->_follower._currentTerm, req.messageId), std::move(action));
  };
  static_assert(std::is_nothrow_move_constructible_v<decltype(commitToMemoryAndResolve)>);

  // Action
  return core->insertAsync(std::move(iter), req.waitForSync)
      .thenValue(std::move(commitToMemoryAndResolve))
      .then([measureTime = std::move(measureTimeGuard)](auto&& res) mutable {
        measureTime.fire();
        auto&& [result, toBeResolved] = res.get();
        // Is is okay to fire here, because commitToMemoryAndResolve has release
        // the guard already.
        toBeResolved.fire();
        return std::move(result);
      });
}

replicated_log::LogFollower::GuardedFollowerData::GuardedFollowerData(
    LogFollower const& self, std::unique_ptr<LogCore> logCore, InMemoryLog inMemoryLog)
    : _follower(self), _inMemoryLog(std::move(inMemoryLog)), _logCore(std::move(logCore)) {}

auto replicated_log::LogFollower::getStatus() const -> LogStatus {
  return _guardedFollowerData.doUnderLock([this](auto const& followerData) {
    if (followerData._logCore == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
    }
    FollowerStatus status;
    status.local = followerData.getLocalStatistics();
    status.leader = _leaderId;
    status.term = _currentTerm;
    return LogStatus{std::move(status)};
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
      ASSERT_OR_THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
    }

    // use a unique ptr because move constructor for multimaps is not noexcept
    auto queue = std::make_unique<WaitForQueue>(
        std::move(followerData._waitForQueue.getLockedGuard().get()));

    auto action = [queue = std::move(queue)]() mutable noexcept {
      std::for_each(queue->begin(), queue->end(), [](auto& pair) {
        pair.second.setException(basics::Exception(TRI_ERROR_REPLICATION_LEADER_CHANGE,
                                                   __FILE__, __LINE__));
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
    -> replicated_log::LogParticipantI::WaitForFuture {
  auto self = _guardedFollowerData.getLockedGuard();
  if (self->_commitIndex >= idx) {
    return futures::Future<std::shared_ptr<QuorumData const>>{
        std::in_place, std::make_shared<QuorumData>(idx, _currentTerm)};
  }
  // emplace might throw a std::bad_alloc but the remainder is noexcept
  // so either you inserted it and or nothing happens
  auto it = self->_waitForQueue.getLockedGuard()->emplace(idx, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}

auto replicated_log::LogFollower::waitForIterator(LogIndex index)
    -> replicated_log::LogParticipantI::WaitForIteratorFuture {
  if (index == LogIndex{0}) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid parameter; log index 0 is invalid");
  }
  return waitFor(index).thenValue([this, self = shared_from_this(), index](auto&& quorum) {
    return getLogIterator(LogIndex{index.value - 1});
  });
}

auto replicated_log::LogFollower::getLogIterator(LogIndex fromIdx) const
    -> std::unique_ptr<LogIterator> {
  return _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData const& data) -> std::unique_ptr<LogIterator> {
        auto const endIdx = data._inMemoryLog.getNextIndex();
        TRI_ASSERT(fromIdx < endIdx);
        return data._inMemoryLog.getIteratorFrom(fromIdx);
      });
}

replicated_log::LogFollower::~LogFollower() {
  _logMetrics->replicatedLogFollowerNumber->fetch_sub(1);
}


auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics() const noexcept
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead.index = _inMemoryLog.getLastIndex();
  result.spearHead.term = _inMemoryLog.getLastTerm();
  return result;
}
