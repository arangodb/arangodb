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

#include "Replication2/ReplicatedLog/PersistedLog.h"

#include <Basics/Exceptions.h>
#include <Basics/Result.h>
#include <Basics/debugging.h>
#include <Basics/voc-errors.h>
#include <Futures/Promise.h>

#include <algorithm>
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
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::LogFollower::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto [result, toBeResolved] = _guardedFollowerData.doUnderLock(
      [&](GuardedFollowerData& self) -> std::pair<AppendEntriesResult, WaitForQueue> {
        if (self._logCore == nullptr) {
          return std::make_pair(AppendEntriesResult(_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::LOST_LOG_CORE, req.messageId),
                                WaitForQueue{});
        }

        if (self._lastRecvMessageId >= req.messageId) {
          return std::make_pair(AppendEntriesResult(_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::MESSAGE_OUTDATED, req.messageId),
                                WaitForQueue{});
        }
        self._lastRecvMessageId = req.messageId;

        if (req.leaderId != _leaderId) {
          return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::INVALID_LEADER_ID, req.messageId},
                                WaitForQueue{});
        }

        // TODO does >= suffice here? Maybe we want to do an atomic operation
        //      before increasing our term
        if (req.leaderTerm != _currentTerm) {
          return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                    AppendEntriesErrorReason::WRONG_TERM, req.messageId},
                                WaitForQueue{});
        }
        // TODO This happily modifies all parameters. Can we refactor that to make it a little nicer?

        if (req.prevLogIndex > LogIndex{0}) {
          auto entry = self._inMemoryLog.getEntryByIndex(req.prevLogIndex);
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

            return std::make_pair(AppendEntriesResult{_currentTerm, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED,
                                                      AppendEntriesErrorReason::NO_PREV_LOG_MATCH, req.messageId},
                                  WaitForQueue{});
          }
        }

        auto res = self._logCore->removeBack(req.prevLogIndex + 1);
        if (!res.ok()) {
          abort();  // TODO abort?
        }

        auto iter =
            ContainerIterator<immer::flex_vector<LogEntry>::const_iterator>(
                req.entries.begin(), req.entries.end());
        // TODO can we make this async?
        res = self._logCore->insert(iter);
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

        return std::make_pair(AppendEntriesResult{_currentTerm, req.messageId}, std::move(toBeResolved));
      });

  for (auto& promise : toBeResolved) {
    // TODO what do we resolve this with? QuorumData is not available on follower
    // TODO execute this in a different context.
    promise.second.setValue(std::shared_ptr<QuorumData>{});
  }

  return std::move(result);
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

replicated_log::LogFollower::LogFollower(ReplicatedLogMetrics& logMetrics,
                                         ParticipantId id, std::unique_ptr<LogCore> logCore,
                                         LogTerm term, ParticipantId leaderId,
                                         replicated_log::InMemoryLog inMemoryLog)
    : _logMetrics(logMetrics),
      _participantId(std::move(id)),
      _leaderId(std::move(leaderId)),
      _currentTerm(term),
      _guardedFollowerData(*this, std::move(logCore), std::move(inMemoryLog)) {}

auto replicated_log::LogFollower::waitFor(LogIndex idx)
    -> replicated_log::LogParticipantI::WaitForFuture {
  auto self = acquireMutex();
  return self->waitFor(idx);
}

auto replicated_log::LogFollower::GuardedFollowerData::getLocalStatistics() const
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.spearHead = _inMemoryLog.getLastIndex();
  return result;
}
