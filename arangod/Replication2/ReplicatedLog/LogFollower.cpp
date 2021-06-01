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

#include "Replication2/ReplicatedLog/LogContextKeys.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/messages.h"
#include "Replication2/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "RestServer/Metrics.h"

#include <Basics/Exceptions.h>
#include <Basics/Result.h>
#include <Basics/debugging.h>
#include <Basics/voc-errors.h>
#include <Futures/Promise.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <utility>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <Basics/application-exit.h>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto const startTime = std::chrono::steady_clock::now();
  auto measureTime = DeferredAction{[startTime, metrics = _logMetrics]() noexcept {
    auto const endTime = std::chrono::steady_clock::now();
    auto const duration =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    metrics->replicatedLogFollowerAppendEntriesRtUs->count(duration.count());
  }};

  struct WaitForQueueResolve {
    WaitForQueueResolve(Guarded<WaitForQueue>::mutex_guard_type guard, LogIndex commitIndex)
        : _guard(std::move(guard)),
          begin(_guard->begin()),
          end(_guard->upper_bound(commitIndex)) {}

    Guarded<WaitForQueue>::mutex_guard_type _guard;
    WaitForQueue::iterator begin;
    WaitForQueue::iterator end;
  };

  auto toBeResolved = std::make_unique<std::optional<WaitForQueueResolve>>();

  auto self = _guardedFollowerData.getLockedGuard();

  if (self->_logCore == nullptr) {
    LOG_CTX("d290d", DEBUG, _logContext)
        << "reject append entries - log core gone";
    return AppendEntriesResult(_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                               AppendEntriesErrorReason::LOST_LOG_CORE, req.messageId);
  }

  if (self->_lastRecvMessageId >= req.messageId) {
    LOG_CTX("d291d", DEBUG, _logContext)
        << "reject append entries - message id out dated: " << req.messageId;
    return AppendEntriesResult(_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                               AppendEntriesErrorReason::MESSAGE_OUTDATED, req.messageId);
  }
  self->_lastRecvMessageId = req.messageId;

  if (req.leaderId != _leaderId) {
    LOG_CTX("a2009", DEBUG, _logContext)
        << "reject append entries - wrong leader, given = " << req.leaderId
        << " current = " << _leaderId;
    return AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                               AppendEntriesErrorReason::INVALID_LEADER_ID, req.messageId};
  }

  // TODO does >= suffice here? Maybe we want to do an atomic operation
  //      before increasing our term
  if (req.leaderTerm != _currentTerm) {
    LOG_CTX("dd7a3", DEBUG, _logContext)
        << "reject append entries - wrong term, given = " << req.leaderTerm
        << ", current = " << _currentTerm;
    return AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                               AppendEntriesErrorReason::WRONG_TERM, req.messageId};
  }
  // TODO This happily modifies all parameters. Can we refactor that to make it a little nicer?

  if (req.prevLogIndex > LogIndex{0}) {
    auto entry = self->_inMemoryLog.getEntryByIndex(req.prevLogIndex);
    if (!entry.has_value() || entry->logTerm() != req.prevLogTerm) {
      // TODO If desired, the protocol can be optimized to reduce the number
      //      of rejected AppendEntries RPCs. For example, when rejecting
      //      an AppendEntries request, the follower can include the term
      //      of the conflicting entry and the first index it stores for
      //      that term. With this information, the leader can decrement
      //      nextIndex to bypass all of the conflicting entries in that
      //      term; one AppendEntries RPC will be required for each term
      //      with conflicting entries, rather than one RPC per entry.
      // from raft-pdf page 7-8
      LOG_CTX("1e86a", TRACE, _logContext)
          << "reject append entries - prev log index/term not matching";
      return AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                 AppendEntriesErrorReason::NO_PREV_LOG_MATCH, req.messageId};
    }
  }

  // prepare the new in memory state
  auto newInMemoryLog = self->_inMemoryLog._log.take(req.prevLogIndex.value);

  auto res = self->_logCore->removeBack(req.prevLogIndex + 1);
  if (!res.ok()) {
    LOG_CTX("f17b8", ERR, _logContext)
        << "failed to remove log entries after " << req.prevLogIndex;
    abort();  // TODO abort?
  }

  // commit the deletion in memory
  // TODO static_assert(std::is_nothrow_move_assignable_v<decltype(newInMemoryLog)>);
  self->_inMemoryLog._log = std::move(newInMemoryLog);
  newInMemoryLog = std::invoke([&]{
    auto transient = self->_inMemoryLog._log.transient();
    transient.append(req.entries.transient());
    return transient.persistent();
  });

  auto iter = std::make_unique<ReplicatedLogIterator>(req.entries);
  auto core = self->_logCore.get();
  return core->insertAsync(std::move(iter), req.waitForSync)
      .thenValue([self = std::move(self), req = std::move(req),
                  newInMemoryLog = std::move(newInMemoryLog),
                  toBeResolved = std::move(toBeResolved)](Result&& res) mutable {
        if (!res.ok()) {
          LOG_CTX("216d8", ERR, self->_self._logContext)
              << "failed to insert log entries: " << res.errorMessage();
          return std::make_pair(
              AppendEntriesResult{
                  self->_self._currentTerm,
                  res.errorNumber(),
                  AppendEntriesErrorReason::PERSISTENCE_FAILURE,
                  req.messageId,
              },
              DeferredAction{});
        }

        // commit the write in memory
        self->_inMemoryLog._log = std::move(newInMemoryLog);

        LOG_CTX("dd72d", TRACE, self->_self._logContext)
            << "appended " << req.entries.size() << " log entries after "
            << req.prevLogIndex << ", leader commit index = " << req.leaderCommit;

        if (self->_commitIndex < req.leaderCommit && !self->_inMemoryLog._log.empty()) {
          self->_commitIndex =
              std::min(req.leaderCommit, self->_inMemoryLog._log.back().logIndex());
          LOG_CTX("1641d", TRACE, self->_self._logContext)
              << "increment commit index: " << self->_commitIndex;

          *toBeResolved = WaitForQueueResolve{self->_waitForQueue.getLockedGuard(),
                                              self->_commitIndex};
          return std::make_pair(AppendEntriesResult{self->_self._currentTerm, req.messageId},
                                DeferredAction([toBeResolved = std::move(toBeResolved)]() noexcept {
                                  for (auto it = toBeResolved->value().begin;
                                       it != toBeResolved->value().end; it++) {
                                    // TODO what do we resolve this with? QuorumData is not available on follower
                                    // TODO execute this in a different context.
                                    if (!it->second.isFulfilled()) {
                                      // This only throws if promise was fulfilled earlier.
                                      it->second.setValue(std::shared_ptr<QuorumData>{});
                                    }
                                  }
                                }));
        }
        return std::make_pair(AppendEntriesResult{self->_self._currentTerm, req.messageId}, DeferredAction{});
      })
      .then([measureTime = std::move(measureTime)](auto&& res) mutable {
        measureTime.fire();
        auto&& [result, toBeResolved] = res.get();
        return std::move(result);
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
  // emplace might throw a std::bad_alloc but the remainder is noexcept
  // so either you inserted it and or nothing happens
  auto it = _waitForQueue.getLockedGuard()->emplace(index, WaitForPromise{});
  auto& promise = it->second;
  auto&& future = promise.getFuture();
  TRI_ASSERT(future.valid());
  return std::move(future);
}


auto replicated_log::LogFollower::waitForIterator(LogIndex index)
-> replicated_log::LogParticipantI::WaitForIteratorFuture {
    TRI_ASSERT(index != LogIndex{0});
    return waitFor(index).thenValue([this, self = shared_from_this(), index](auto&& quorum) {
        // TODO what if the data was compacted after the commit index reached index?
        return getLogIterator(LogIndex{index.value - 1});
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

auto replicated_log::LogFollower::resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> {
  return _guardedFollowerData.doUnderLock([this](GuardedFollowerData& followerData) {
    LOG_CTX("838fe", DEBUG, _logContext) << "follower resign";
    if (followerData._logCore == nullptr) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
    }

    // Use swap for multimap because its noexcept
    // use a unique ptr because move constructor for multimaps is not noexcept
    auto queue = std::make_unique<WaitForQueue>();
    // Guarantee that the code below never throws an exception
    return std::invoke([&]() noexcept {
      using std::swap;
      {
        auto guard = followerData._waitForQueue.getLockedGuard();
        swap(*queue, guard.get());
      }
      auto lambda = [queue = std::move(queue)]() mutable noexcept {
        for (auto& [idx, promise] : *queue) {
          promise.setException(basics::Exception(TRI_ERROR_REPLICATION_LEADER_CHANGE,
                                                 __FILE__, __LINE__));
        }
      };
      static_assert(std::is_nothrow_constructible_v<DeferredAction, decltype(lambda)&&>);
      auto action = DeferredAction{std::move(lambda)};
      return std::make_tuple(std::move(followerData._logCore), std::move(action));
    });
  });
}

replicated_log::LogFollower::LogFollower(LogContext const& logContext,
                                         std::shared_ptr<ReplicatedLogMetrics> logMetrics,
                                         ParticipantId id, std::unique_ptr<LogCore> logCore,
                                         LogTerm term, ParticipantId leaderId,
                                         replicated_log::InMemoryLog inMemoryLog)
    : _logMetrics(std::move(logMetrics)),
      _participantId(std::move(id)),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)),
      _logContext(logContext.with<logContextKeyLogComponent>("follower")
                      .with<logContextKeyLeaderId>(_leaderId)
                      .with<logContextKeyTerm>(term)) {
  _logMetrics->replicatedLogFollowerNumber->fetch_add(1);
}

auto replicated_log::LogFollower::waitFor(LogIndex idx)
    -> replicated_log::LogParticipantI::WaitForFuture {
  auto self = acquireMutex();
  return self->waitFor(idx);
}

auto replicated_log::LogFollower::getLogIterator(LogIndex fromIdx) const
    -> std::unique_ptr<LogIterator> {
  return _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData const& data) -> std::unique_ptr<LogIterator> {
        auto const endIdx = data._inMemoryLog.getNextIndex();
        TRI_ASSERT(fromIdx < endIdx);
        auto log = data._inMemoryLog._log.drop(fromIdx.value);
        return std::make_unique<ReplicatedLogIterator>(std::move(log));
      });
}

replicated_log::LogFollower::~LogFollower() {
  // TODO It'd be more accurate to do this in resign(), and here only conditionally
  //      depending on whether we still own the LogCore.
  _logMetrics->replicatedLogFollowerNumber->fetch_sub(1);
}

auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics() const noexcept
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = _inMemoryLog.getLastIndex();
  return result;
}
