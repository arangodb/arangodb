////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "LogLeader.h"

#include <Basics/Exceptions.h>
#include <Basics/Guarded.h>
#include <Basics/StringUtils.h>
#include <Basics/application-exit.h>
#include <Basics/debugging.h>
#include <Basics/system-compiler.h>
#include <Basics/voc-errors.h>
#include <Containers/ImmerMemoryPolicy.h>
#include <Futures/Future.h>
#include <Futures/Try.h>
#include <Logger/LogMacros.h>
#include <Logger/Logger.h>
#include <Logger/LoggerStream.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "Basics/ErrorCode.h"
#include "Futures/Promise-inl.h"
#include "Futures/Promise.h"
#include "Futures/Unit.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/DeferredExecution.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Metrics/Gauge.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "immer/detail/iterator_facade.hpp"
#include "immer/detail/rbts/rrbtree_iterator.hpp"

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

replicated_log::LogLeader::LogLeader(
    LoggerContext logContext, std::shared_ptr<ReplicatedLogMetrics> logMetrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    LogConfig config, ParticipantId id, LogTerm term, LogIndex firstIndex,
    InMemoryLog inMemoryLog)
    : _logContext(std::move(logContext)),
      _logMetrics(std::move(logMetrics)),
      _options(std::move(options)),
      _config(config),
      _id(std::move(id)),
      _currentTerm(term),
      _firstIndexOfCurrentTerm(firstIndex),
      _guardedLeaderData(*this, std::move(inMemoryLog)) {
  _logMetrics->replicatedLogLeaderNumber->fetch_add(1);
}

replicated_log::LogLeader::~LogLeader() {
  _logMetrics->replicatedLogLeaderNumber->fetch_sub(1);
  if (auto queueEmpty =
          _guardedLeaderData.getLockedGuard()->_waitForQueue.empty();
      !queueEmpty) {
    TRI_ASSERT(false) << "expected wait-for-queue to be empty";
    LOG_CTX("ce7f7", ERR, _logContext) << "expected wait-for-queue to be empty";
  }
}

auto replicated_log::LogLeader::instantiateFollowers(
    LoggerContext const& logContext,
    std::vector<std::shared_ptr<AbstractFollower>> const& followers,
    std::shared_ptr<LocalFollower> const& localFollower,
    TermIndexPair lastEntry)
    -> std::unordered_map<ParticipantId, std::shared_ptr<FollowerInfo>> {
  auto initLastIndex = lastEntry.index.saturatedDecrement();

  std::unordered_map<ParticipantId, std::shared_ptr<FollowerInfo>>
      followers_map;
  followers_map.reserve(followers.size() + 1);
  followers_map.emplace(
      localFollower->getParticipantId(),
      std::make_shared<FollowerInfo>(localFollower, lastEntry, logContext));
  for (auto const& impl : followers) {
    auto const& [it, inserted] = followers_map.emplace(
        impl->getParticipantId(),
        std::make_shared<FollowerInfo>(
            impl, TermIndexPair{LogTerm{0}, initLastIndex}, logContext));
    TRI_ASSERT(inserted) << "duplicate participant id: "
                         << impl->getParticipantId();
  }
  return followers_map;
}

namespace {
auto delayedFuture(std::chrono::steady_clock::duration duration)
    -> futures::Future<futures::Unit> {
  if (SchedulerFeature::SCHEDULER) {
    return SchedulerFeature::SCHEDULER->delay(duration);
  }

  // std::this_thread::sleep_for(duration);
  return futures::Future<futures::Unit>{std::in_place};
}
}  // namespace

void replicated_log::LogLeader::handleResolvedPromiseSet(
    ResolvedPromiseSet resolvedPromises,
    std::shared_ptr<ReplicatedLogMetrics> const& logMetrics) {
  auto const commitTp = InMemoryLogEntry::clock::now();
  for (auto const& it : resolvedPromises._commitedLogEntries) {
    using namespace std::chrono_literals;
    auto const entryDuration = commitTp - it.insertTp();
    logMetrics->replicatedLogInsertsRtt->count(entryDuration / 1us);
  }

  for (auto& promise : resolvedPromises._set) {
    TRI_ASSERT(promise.second.valid());
    promise.second.setValue(resolvedPromises.result);
  }
}

void replicated_log::LogLeader::executeAppendEntriesRequests(
    std::vector<std::optional<PreparedAppendEntryRequest>> requests,
    std::shared_ptr<ReplicatedLogMetrics> const& logMetrics) {
  for (auto& it : requests) {
    if (it.has_value()) {
      delayedFuture(it->_executionDelay)
          .thenFinal([it = std::move(it), logMetrics](auto&&) mutable {
            auto follower = it->_follower.lock();
            auto logLeader = it->_parentLog.lock();
            if (logLeader == nullptr || follower == nullptr) {
              LOG_TOPIC("de312", TRACE, Logger::REPLICATION2)
                  << "parent log already gone, not sending any more "
                     "AppendEntryRequests";
              return;
            }

            auto [request, lastIndex] =
                logLeader->_guardedLeaderData.doUnderLock(
                    [&](auto const& self) {
                      auto lastAvailableIndex =
                          self._inMemoryLog.getLastTermIndexPair();
                      LOG_CTX("71801", TRACE, follower->logContext)
                          << "last acked index = " << follower->lastAckedEntry
                          << ", current index = " << lastAvailableIndex
                          << ", last acked commit index = "
                          << follower->lastAckedCommitIndex
                          << ", current commit index = " << self._commitIndex
                          << ", last acked lci = " << follower->lastAckedLCI
                          << ", current lci = " << self._largestCommonIndex;
                      // We can only get here if there is some new information
                      // for this follower
                      TRI_ASSERT(
                          follower->lastAckedEntry.index !=
                              lastAvailableIndex.index ||
                          self._commitIndex != follower->lastAckedCommitIndex ||
                          self._largestCommonIndex != follower->lastAckedLCI);

                      return self.createAppendEntriesRequest(
                          *follower, lastAvailableIndex);
                    });

            auto messageId = request.messageId;
            LOG_CTX("1b0ec", TRACE, follower->logContext)
                << "sending append entries, messageId = " << messageId;

            // We take the start time here again to have a more precise
            // measurement. (And do not use follower._lastRequestStartTP)
            // TODO really needed?
            auto startTime = std::chrono::steady_clock::now();
            // Capture a weak pointer `parentLog` that will be locked
            // when the request returns. If the locking is successful
            // we are still in the same term.
            follower->_impl->appendEntries(std::move(request))
                .thenFinal([weakParentLog = it->_parentLog,
                            followerWeak = it->_follower, lastIndex = lastIndex,
                            currentCommitIndex = request.leaderCommit,
                            currentLCI = request.largestCommonIndex,
                            currentTerm = logLeader->_currentTerm,
                            messageId = messageId, startTime,
                            logMetrics =
                                logMetrics](futures::Try<AppendEntriesResult>&&
                                                res) noexcept {
                  // This has to remain noexcept, because the code below is not
                  // exception safe
                  auto const endTime = std::chrono::steady_clock::now();

                  auto self = weakParentLog.lock();
                  auto follower = followerWeak.lock();
                  if (self != nullptr && follower != nullptr) {
                    using namespace std::chrono_literals;
                    auto const duration = endTime - startTime;
                    self->_logMetrics->replicatedLogAppendEntriesRttUs->count(
                        duration / 1us);
                    LOG_CTX("8ff44", TRACE, follower->logContext)
                        << "received append entries response, messageId = "
                        << messageId;
                    auto [preparedRequests, resolvedPromises] = std::invoke(
                        [&]() -> std::pair<std::vector<std::optional<
                                               PreparedAppendEntryRequest>>,
                                           ResolvedPromiseSet> {
                          auto guarded = self->acquireMutex();
                          if (!guarded->_didResign) {
                            return guarded->handleAppendEntriesResponse(
                                *follower, lastIndex, currentCommitIndex,
                                currentLCI, currentTerm, std::move(res),
                                endTime - startTime, messageId);
                          } else {
                            LOG_CTX("da116", DEBUG, follower->logContext)
                                << "received response from follower but leader "
                                   "already resigned, messageId = "
                                << messageId;
                          }
                          return {};
                        });

                    handleResolvedPromiseSet(std::move(resolvedPromises),
                                             logMetrics);
                    executeAppendEntriesRequests(std::move(preparedRequests),
                                                 logMetrics);
                  } else {
                    if (follower == nullptr) {
                      LOG_TOPIC("6f490", DEBUG, Logger::REPLICATION2)
                          << "follower already gone.";
                    } else {
                      LOG_CTX("de300", DEBUG, follower->logContext)
                          << "parent log already gone, messageId = "
                          << messageId;
                    }
                  }
                });
          });
    }
  }
}

auto replicated_log::LogLeader::construct(
    LogConfig config, std::unique_ptr<LogCore> logCore,
    std::vector<std::shared_ptr<AbstractFollower>> const& followers,
    std::shared_ptr<ParticipantsConfig const> participantsConfig,
    ParticipantId id, LogTerm term, LoggerContext const& logContext,
    std::shared_ptr<ReplicatedLogMetrics> logMetrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options)
    -> std::shared_ptr<LogLeader> {
  if (ADB_UNLIKELY(logCore == nullptr)) {
    auto followerIds = std::vector<std::string>{};
    std::transform(followers.begin(), followers.end(),
                   std::back_inserter(followerIds),
                   [](auto const& follower) -> std::string {
                     return follower->getParticipantId();
                   });
    auto message = basics::StringUtils::concatT(
        "LogCore missing when constructing LogLeader, leader id: ", id,
        "term: ", term, "writeConcern: ", config.writeConcern,
        "followers: ", basics::StringUtils::join(followerIds, ", "));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(message));
  }

  // Workaround to be able to use make_shared, while LogLeader's constructor
  // is actually protected.
  struct MakeSharedLogLeader : LogLeader {
   public:
    MakeSharedLogLeader(
        LoggerContext logContext,
        std::shared_ptr<ReplicatedLogMetrics> logMetrics,
        std::shared_ptr<ReplicatedLogGlobalSettings const> options,
        LogConfig config, ParticipantId id, LogTerm term,
        LogIndex firstIndexOfCurrentTerm, InMemoryLog inMemoryLog)
        : LogLeader(std::move(logContext), std::move(logMetrics),
                    std::move(options), config, std::move(id), term,
                    firstIndexOfCurrentTerm, std::move(inMemoryLog)) {}
  };

  auto log = InMemoryLog::loadFromLogCore(*logCore);
  auto const lastIndex = log.getLastTermIndexPair();
  // if this assertion triggers there is an entry present in the log
  // that has the current term. Did create a different leader with the same term
  // in your test?
  TRI_ASSERT(lastIndex.term != term);

  // Note that although we add an entry to establish our leadership
  // we do still want to use the unchanged lastIndex to initialize
  // our followers with, as none of them can possibly have this entry.
  // This is particularly important for the LocalFollower, which blindly
  // accepts appendEntriesRequests, and we would thus forget persisting this
  // entry on the leader!

  auto commonLogContext =
      logContext.with<logContextKeyTerm>(term).with<logContextKeyLeaderId>(id);

  auto leader = std::make_shared<MakeSharedLogLeader>(
      commonLogContext.with<logContextKeyLogComponent>("leader"),
      std::move(logMetrics), std::move(options), config, std::move(id), term,
      lastIndex.index + 1u, log);
  auto localFollower = std::make_shared<LocalFollower>(
      *leader,
      commonLogContext.with<logContextKeyLogComponent>("local-follower"),
      std::move(logCore), lastIndex);

  TRI_ASSERT(participantsConfig != nullptr);
  {
    auto leaderDataGuard = leader->acquireMutex();

    leaderDataGuard->_follower = instantiateFollowers(
        commonLogContext, followers, localFollower, lastIndex);
    leaderDataGuard->activeParticipantsConfig = participantsConfig;
    leader->_localFollower = std::move(localFollower);
    TRI_ASSERT(leaderDataGuard->_follower.size() >= config.writeConcern)
        << "actual followers: " << leaderDataGuard->_follower.size()
        << " writeConcern: " << config.writeConcern;
    TRI_ASSERT(leaderDataGuard->_follower.size() ==
               leaderDataGuard->activeParticipantsConfig->participants.size());
    TRI_ASSERT(std::all_of(leaderDataGuard->_follower.begin(),
                           leaderDataGuard->_follower.end(),
                           [&](auto const& it) {
                             return leaderDataGuard->activeParticipantsConfig
                                 ->participants.contains(it.first);
                           }));
  }

  leader->establishLeadership(std::move(participantsConfig));

  return leader;
}

auto replicated_log::LogLeader::acquireMutex() -> LogLeader::Guard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::acquireMutex() const -> LogLeader::ConstGuard {
  return _guardedLeaderData.getLockedGuard();
}

auto replicated_log::LogLeader::resign() && -> std::tuple<
    std::unique_ptr<LogCore>, DeferredAction> {
  return _guardedLeaderData.doUnderLock([this, &localFollower = *_localFollower,
                                         &participantId = _id](
                                            GuardedLeaderData& leaderData) {
    if (leaderData._didResign) {
      LOG_CTX("5d3b8", ERR, _logContext)
          << "Leader " << participantId << " already resigned!";
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
    }

    // WARNING! This stunt is here to make things exception safe.
    // The move constructor of std::multimap is **not** noexcept.
    // Thus we have to make a new map unique and use std::swap to
    // transfer the content. And then move the unique_ptr into
    // the lambda.
    auto queue =
        std::make_unique<WaitForQueue>(std::move(leaderData._waitForQueue));

    auto action = [promises = std::move(queue)]() mutable noexcept {
      for (auto& [idx, promise] : *promises) {
        // Check this to make sure that setException does not throw
        if (!promise.isFulfilled()) {
          promise.setException(ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE));
        }
      }
    };

    LOG_CTX("8696f", DEBUG, _logContext) << "resign";
    leaderData._didResign = true;
    static_assert(
        std::is_nothrow_constructible_v<
            DeferredAction, std::add_rvalue_reference_t<decltype(action)>>);
    static_assert(noexcept(std::declval<LocalFollower&&>().resign()));
    return std::make_tuple(std::move(localFollower).resign(),
                           DeferredAction(std::move(action)));
  });
}

auto replicated_log::LogLeader::readReplicatedEntryByIndex(LogIndex idx) const
    -> std::optional<PersistingLogEntry> {
  return _guardedLeaderData.doUnderLock(
      [&idx](auto& leaderData) -> std::optional<PersistingLogEntry> {
        if (leaderData._didResign) {
          throw ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
        }
        if (auto entry = leaderData._inMemoryLog.getEntryByIndex(idx);
            entry.has_value() &&
            entry->entry().logIndex() <= leaderData._commitIndex) {
          return entry->entry();
        } else {
          return std::nullopt;
        }
      });
}

auto replicated_log::LogLeader::getStatus() const -> LogStatus {
  return _guardedLeaderData.doUnderLock([term = _currentTerm](
                                            GuardedLeaderData const&
                                                leaderData) {
    if (leaderData._didResign) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
    }
    LeaderStatus status;
    status.local = leaderData.getLocalStatistics();
    status.term = term;
    status.largestCommonIndex = leaderData._largestCommonIndex;
    status.lastCommitStatus = leaderData._lastCommitFailReason;
    status.leadershipEstablished = leaderData._leadershipEstablished;
    status.activeParticipantsConfig = *leaderData.activeParticipantsConfig;
    if (auto const config = leaderData.committedParticipantsConfig;
        config != nullptr) {
      status.committedParticipantsConfig = *config;
    }
    for (auto const& [pid, f] : leaderData._follower) {
      auto lastRequestLatencyMS =
          std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
              f->_lastRequestLatency);
      auto state = std::invoke([&, &f = f] {
        switch (f->_state) {
          case FollowerInfo::State::ERROR_BACKOFF:
            return FollowerState::withErrorBackoff(
                std::chrono::duration_cast<
                    std::chrono::duration<double, std::milli>>(
                    f->_errorBackoffEndTP - std::chrono::steady_clock::now()),
                f->numErrorsSinceLastAnswer);
          case FollowerInfo::State::REQUEST_IN_FLIGHT:
            return FollowerState::withRequestInFlight(
                std::chrono::duration_cast<
                    std::chrono::duration<double, std::milli>>(
                    std::chrono::steady_clock::now() - f->_lastRequestStartTP));
          default:
            return FollowerState::withUpToDate();
        }
      });
      auto const& participantId = f->_impl->getParticipantId();
      TRI_ASSERT(pid == participantId);
      TRI_ASSERT(!pid.empty());
      status.follower.emplace(
          participantId,
          FollowerStatistics{
              LogStatistics{f->lastAckedEntry, f->lastAckedCommitIndex},
              f->lastErrorReason, lastRequestLatencyMS, state});
    }

    status.commitLagMS = leaderData.calculateCommitLag();
    return LogStatus{std::move(status)};
  });
}

auto replicated_log::LogLeader::getQuickStatus() const -> QuickLogStatus {
  return _guardedLeaderData.doUnderLock(
      [term = _currentTerm](GuardedLeaderData const& leaderData) {
        if (leaderData._didResign) {
          throw ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
        }
        auto commitFailReason = std::optional<CommitFailReason>{};
        if (leaderData.calculateCommitLag() > std::chrono::seconds{20}) {
          commitFailReason = leaderData._lastCommitFailReason;
        }
        return QuickLogStatus{
            .role = ParticipantRole::kLeader,
            .term = term,
            .local = leaderData.getLocalStatistics(),
            .leadershipEstablished = leaderData._leadershipEstablished,
            .commitFailReason = commitFailReason,
            .activeParticipantsConfig = leaderData.activeParticipantsConfig,
            .committedParticipantsConfig =
                leaderData.committedParticipantsConfig};
      });
}

auto replicated_log::LogLeader::insert(LogPayload payload, bool waitForSync)
    -> LogIndex {
  auto index =
      insert(std::move(payload), waitForSync, doNotTriggerAsyncReplication);
  triggerAsyncReplication();
  return index;
}

auto replicated_log::LogLeader::insert(LogPayload payload, bool waitForSync,
                                       DoNotTriggerAsyncReplication)
    -> LogIndex {
  auto const insertTp = InMemoryLogEntry::clock::now();
  // Currently we use a mutex. Is this the only valid semantic?
  return _guardedLeaderData.doUnderLock([&](GuardedLeaderData& leaderData) {
    return leaderData.insertInternal(std::move(payload), waitForSync, insertTp);
  });
}

auto replicated_log::LogLeader::GuardedLeaderData::insertInternal(
    std::optional<LogPayload> payload, bool waitForSync,
    std::optional<InMemoryLogEntry::clock::time_point> insertTp) -> LogIndex {
  if (this->_didResign) {
    throw ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
  }
  auto const index = this->_inMemoryLog.getNextIndex();
  auto const payloadSize = payload.has_value() ? payload->byteSize() : 0;
  auto logEntry = InMemoryLogEntry(
      PersistingLogEntry(_self._currentTerm, index, std::move(payload)),
      waitForSync);
  logEntry.setInsertTp(insertTp.has_value() ? *insertTp
                                            : InMemoryLogEntry::clock::now());
  this->_inMemoryLog.appendInPlace(_self._logContext, std::move(logEntry));
  _self._logMetrics->replicatedLogInsertsBytes->count(payloadSize);
  return index;
}

auto replicated_log::LogLeader::waitFor(LogIndex index) -> WaitForFuture {
  return _guardedLeaderData.doUnderLock([index](auto& leaderData) {
    if (leaderData._didResign) {
      auto promise = WaitForPromise{};
      promise.setException(ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE));
      return promise.getFuture();
    }
    if (leaderData._commitIndex >= index) {
      return futures::Future<WaitForResult>{
          std::in_place, leaderData._commitIndex, leaderData._lastQuorum};
    }
    auto it = leaderData._waitForQueue.emplace(index, WaitForPromise{});
    auto& promise = it->second;
    auto&& future = promise.getFuture();
    TRI_ASSERT(future.valid());
    return std::move(future);
  });
}

auto replicated_log::LogLeader::getParticipantId() const noexcept
    -> ParticipantId const& {
  return _id;
}

auto replicated_log::LogLeader::triggerAsyncReplication() -> void {
  auto preparedRequests = _guardedLeaderData.doUnderLock([](auto& leaderData) {
    if (leaderData._didResign) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
    }
    return leaderData.prepareAppendEntries();
  });
  executeAppendEntriesRequests(std::move(preparedRequests), _logMetrics);
}

auto replicated_log::LogLeader::GuardedLeaderData::updateCommitIndexLeader(
    LogIndex newIndex, std::shared_ptr<QuorumData> quorum)
    -> ResolvedPromiseSet {
  LOG_CTX("a9a7e", TRACE, _self._logContext)
      << "updating commit index to " << newIndex << " with quorum "
      << quorum->quorum;
  auto oldIndex = _commitIndex;

  TRI_ASSERT(_commitIndex < newIndex)
      << "_commitIndex == " << _commitIndex << ", newIndex == " << newIndex;
  _commitIndex = newIndex;
  _lastQuorum = quorum;

  try {
    WaitForQueue toBeResolved;
    auto const end = _waitForQueue.upper_bound(_commitIndex);
    for (auto it = _waitForQueue.begin(); it != end;) {
      LOG_CTX("37d9d", TRACE, _self._logContext)
          << "resolving promise for index " << it->first;
      toBeResolved.insert(_waitForQueue.extract(it++));
    }
    return ResolvedPromiseSet{std::move(toBeResolved),
                              WaitForResult(newIndex, std::move(quorum)),
                              _inMemoryLog.slice(oldIndex, newIndex + 1)};
  } catch (std::exception const& e) {
    // If those promises are not fulfilled we can not continue.
    // Note that the move constructor of std::multi_map is not noexcept.
    LOG_CTX("e7a4e", FATAL, _self._logContext)
        << "failed to fulfill replication promises due to exception; system "
           "can not continue. message: "
        << e.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    // If those promises are not fulfilled we can not continue.
    // Note that the move constructor of std::multi_map is not noexcept.
    LOG_CTX("c0bbb", FATAL, _self._logContext)
        << "failed to fulfill replication promises due to exception; system "
           "can not continue";
    FATAL_ERROR_EXIT();
  }
}

auto replicated_log::LogLeader::GuardedLeaderData::prepareAppendEntries()
    -> std::vector<std::optional<PreparedAppendEntryRequest>> {
  auto appendEntryRequests =
      std::vector<std::optional<PreparedAppendEntryRequest>>{};
  appendEntryRequests.reserve(_follower.size());
  std::transform(
      _follower.begin(), _follower.end(),
      std::back_inserter(appendEntryRequests),
      [this](auto& follower) { return prepareAppendEntry(follower.second); });
  return appendEntryRequests;
}

auto replicated_log::LogLeader::GuardedLeaderData::prepareAppendEntry(
    std::shared_ptr<FollowerInfo> follower)
    -> std::optional<PreparedAppendEntryRequest> {
  if (follower->_state != FollowerInfo::State::IDLE) {
    LOG_CTX("1d7b6", TRACE, follower->logContext)
        << "request in flight - skipping";
    return std::nullopt;  // wait for the request to return
  }

  auto const lastAvailableIndex = _inMemoryLog.getLastTermIndexPair();
  LOG_CTX("8844a", TRACE, follower->logContext)
      << "last acked index = " << follower->lastAckedEntry
      << ", current index = " << lastAvailableIndex
      << ", last acked commit index = " << follower->lastAckedCommitIndex
      << ", current commit index = " << _commitIndex
      << ", last acked lci = " << follower->lastAckedLCI
      << ", current lci = " << _largestCommonIndex;
  if (follower->lastAckedEntry.index == lastAvailableIndex.index &&
      _commitIndex == follower->lastAckedCommitIndex &&
      _largestCommonIndex == follower->lastAckedLCI) {
    LOG_CTX("74b71", TRACE, follower->logContext) << "up to date";
    return std::nullopt;  // nothing to replicate
  }

  auto const executionDelay = std::invoke([&] {
    using namespace std::chrono_literals;
    if (follower->numErrorsSinceLastAnswer > 0) {
      // Capped exponential backoff. Wait for 100us, 200us, 400us, ...
      // until at most 100us * 2 ** 17 == 13.11s.
      auto executionDelay =
          100us *
          (1u << std::min(follower->numErrorsSinceLastAnswer, std::size_t{17}));
      LOG_CTX("2a6f7", DEBUG, follower->logContext)
          << follower->numErrorsSinceLastAnswer
          << " requests failed, last one was " << follower->lastSentMessageId
          << " - waiting " << executionDelay / 1ms
          << "ms before sending next message.";
      follower->_state = FollowerInfo::State::ERROR_BACKOFF;
      follower->_errorBackoffEndTP =
          std::chrono::steady_clock::now() + executionDelay;
      return executionDelay;
    } else {
      follower->_state = FollowerInfo::State::PREPARE;
      return 0us;
    }
  });

  return PreparedAppendEntryRequest{_self.shared_from_this(),
                                    std::move(follower), executionDelay};
}

auto replicated_log::LogLeader::GuardedLeaderData::createAppendEntriesRequest(
    replicated_log::LogLeader::FollowerInfo& follower,
    TermIndexPair const& lastAvailableIndex) const
    -> std::pair<AppendEntriesRequest, TermIndexPair> {
  auto const lastAcked =
      _inMemoryLog.getEntryByIndex(follower.lastAckedEntry.index);

  AppendEntriesRequest req;
  req.leaderCommit = _commitIndex;
  req.largestCommonIndex = _largestCommonIndex;
  req.leaderTerm = _self._currentTerm;
  req.leaderId = _self._id;
  req.waitForSync = _self._config.waitForSync;
  req.messageId = ++follower.lastSentMessageId;

  follower._state = FollowerInfo::State::REQUEST_IN_FLIGHT;
  follower._lastRequestStartTP = std::chrono::steady_clock::now();

  if (lastAcked) {
    req.prevLogEntry.index = lastAcked->entry().logIndex();
    req.prevLogEntry.term = lastAcked->entry().logTerm();
    TRI_ASSERT(req.prevLogEntry.index == follower.lastAckedEntry.index);
  } else {
    req.prevLogEntry.index = LogIndex{0};
    req.prevLogEntry.term = LogTerm{0};
  }

  {
    auto it = getInternalLogIterator(follower.lastAckedEntry.index + 1);
    auto transientEntries = decltype(req.entries)::transient_type{};
    auto sizeCounter = std::size_t{0};
    while (auto entry = it->next()) {
      req.waitForSync |= entry->getWaitForSync();

      transientEntries.push_back(InMemoryLogEntry(*entry));
      sizeCounter += entry->entry().approxByteSize();

      if (sizeCounter >= _self._options->_thresholdNetworkBatchSize) {
        break;
      }
    }
    req.entries = std::move(transientEntries).persistent();
  }

  auto isEmptyAppendEntries = req.entries.empty();
  auto lastIndex = isEmptyAppendEntries
                       ? lastAvailableIndex
                       : req.entries.back().entry().logTermIndexPair();

  LOG_CTX("af3c6", TRACE, follower.logContext)
      << "creating append entries request with " << req.entries.size()
      << " entries , prevLogEntry.term = " << req.prevLogEntry.term
      << ", prevLogEntry.index = " << req.prevLogEntry.index
      << ", leaderCommit = " << req.leaderCommit
      << ", lci = " << req.largestCommonIndex << ", msg-id = " << req.messageId;

  return std::make_pair(std::move(req), lastIndex);
}

auto replicated_log::LogLeader::GuardedLeaderData::handleAppendEntriesResponse(
    FollowerInfo& follower, TermIndexPair lastIndex,
    LogIndex currentCommitIndex, LogIndex currentLCI, LogTerm currentTerm,
    futures::Try<AppendEntriesResult>&& res,
    std::chrono::steady_clock::duration latency, MessageId messageId)
    -> std::pair<std::vector<std::optional<PreparedAppendEntryRequest>>,
                 ResolvedPromiseSet> {
  if (currentTerm != _self._currentTerm) {
    LOG_CTX("7ab2e", WARN, follower.logContext)
        << "received append entries response with wrong term: " << currentTerm;
    return {};
  }

  ResolvedPromiseSet toBeResolved;

  follower._lastRequestLatency = latency;

  if (follower.lastSentMessageId == messageId) {
    LOG_CTX("35a32", TRACE, follower.logContext)
        << "received message " << messageId << " - no other requests in flight";
    // there is no request in flight currently
    follower._state = FollowerInfo::State::IDLE;
  }
  if (res.hasValue()) {
    auto& response = res.get();
    TRI_ASSERT(messageId == response.messageId)
        << messageId << " vs. " << response.messageId;
    if (follower.lastSentMessageId == response.messageId) {
      LOG_CTX("35134", TRACE, follower.logContext)
          << "received append entries response, messageId = "
          << response.messageId
          << ", errorCode = " << to_string(response.errorCode)
          << ", reason  = " << to_string(response.reason.error);

      follower.lastErrorReason = response.reason;
      if (response.isSuccess()) {
        follower.numErrorsSinceLastAnswer = 0;
        follower.lastAckedEntry = lastIndex;
        follower.lastAckedCommitIndex = currentCommitIndex;
        follower.lastAckedLCI = currentLCI;
        toBeResolved = checkCommitIndex();
      } else {
        TRI_ASSERT(response.reason.error !=
                   AppendEntriesErrorReason::ErrorType::kNone);
        switch (response.reason.error) {
          case AppendEntriesErrorReason::ErrorType::kNoPrevLogMatch:
            follower.numErrorsSinceLastAnswer = 0;
            TRI_ASSERT(response.conflict.has_value());
            follower.lastAckedEntry.index =
                response.conflict.value().index.saturatedDecrement();
            LOG_CTX("33c6d", DEBUG, follower.logContext)
                << "reset last acked index to " << follower.lastAckedEntry;
            break;
          default:
            LOG_CTX("1bd0b", DEBUG, follower.logContext)
                << "received error from follower, reason = "
                << to_string(response.reason.error)
                << " message id = " << messageId;
            ++follower.numErrorsSinceLastAnswer;
        }
      }
    } else {
      LOG_CTX("056a8", DEBUG, follower.logContext)
          << "received outdated response from follower "
          << follower._impl->getParticipantId() << ": " << response.messageId
          << ", expected " << messageId << ", latest "
          << follower.lastSentMessageId;
    }
  } else if (res.hasException()) {
    ++follower.numErrorsSinceLastAnswer;
    follower.lastErrorReason = {
        AppendEntriesErrorReason::ErrorType::kCommunicationError};
    try {
      res.throwIfFailed();
    } catch (std::exception const& e) {
      follower.lastErrorReason.details = e.what();
      LOG_CTX("e094b", INFO, follower.logContext)
          << "exception in appendEntries to follower "
          << follower._impl->getParticipantId() << ": " << e.what();
    } catch (...) {
      LOG_CTX("05608", INFO, follower.logContext)
          << "exception in appendEntries to follower "
          << follower._impl->getParticipantId() << ".";
    }
  } else {
    LOG_CTX("dc441", FATAL, follower.logContext)
        << "in appendEntries to follower " << follower._impl->getParticipantId()
        << ", result future has neither value nor exception.";
    TRI_ASSERT(false);
    FATAL_ERROR_EXIT();
  }

  // try sending the next batch
  return std::make_pair(prepareAppendEntries(), std::move(toBeResolved));
}

auto replicated_log::LogLeader::GuardedLeaderData::getInternalLogIterator(
    LogIndex firstIdx) const
    -> std::unique_ptr<TypedLogIterator<InMemoryLogEntry>> {
  auto const endIdx = _inMemoryLog.getLastTermIndexPair().index + 1;
  TRI_ASSERT(firstIdx <= endIdx);
  return _inMemoryLog.getMemtryIteratorFrom(firstIdx);
}

auto replicated_log::LogLeader::GuardedLeaderData::getCommittedLogIterator(
    LogIndex firstIndex) const -> std::unique_ptr<LogRangeIterator> {
  auto const endIdx = _inMemoryLog.getNextIndex();
  TRI_ASSERT(firstIndex < endIdx);
  // return an iterator for the range [firstIndex, _commitIndex + 1)
  return _inMemoryLog.getIteratorRange(firstIndex, _commitIndex + 1);
}

/*
 * Collects last acknowledged term/index pairs from all followers.
 * While doing so, it calculates the largest common index, which is
 * the lowest acknowledged index of all followers.
 * No followers are filtered out at this step.
 */
auto replicated_log::LogLeader::GuardedLeaderData::collectFollowerIndexes()
    const
    -> std::pair<LogIndex, std::vector<algorithms::ParticipantStateTuple>> {
  auto largestCommonIndex = _commitIndex;
  std::vector<algorithms::ParticipantStateTuple> indexes;
  indexes.reserve(_follower.size());
  for (auto const& [pid, follower] : _follower) {
    // The lastAckedEntry is the last index/term pair that we sent that this
    // follower acknowledged - means we sent it. And we must not have entries
    // in our log with a term newer than currentTerm, which could have been
    // sent to a follower.
    TRI_ASSERT(follower->lastAckedEntry.term <= this->_self._currentTerm);

    auto flags = activeParticipantsConfig->participants.find(pid);
    TRI_ASSERT(flags != std::end(activeParticipantsConfig->participants));

    indexes.emplace_back(algorithms::ParticipantStateTuple{
        .lastAckedEntry = follower->lastAckedEntry,
        .id = follower->_impl->getParticipantId(),
        .failed = false,
        .flags = flags->second});

    largestCommonIndex =
        std::min(largestCommonIndex, follower->lastAckedCommitIndex);
  }

  return {largestCommonIndex, std::move(indexes)};
}

auto replicated_log::LogLeader::GuardedLeaderData::checkCommitIndex()
    -> ResolvedPromiseSet {
  auto [newLargestCommonIndex, indexes] = collectFollowerIndexes();

  if (newLargestCommonIndex != _largestCommonIndex) {
    // This assertion is no longer true, as followers can now be added.
    // TRI_ASSERT(newLargestCommonIndex > _largestCommonIndex);
    LOG_CTX("851bb", TRACE, _self._logContext)
        << "largest common index went from " << _largestCommonIndex << " to "
        << newLargestCommonIndex;
    _largestCommonIndex = newLargestCommonIndex;
  }

  auto [newCommitIndex, commitFailReason, quorum] =
      algorithms::calculateCommitIndex(
          indexes,
          algorithms::CalculateCommitIndexOptions{
              _self._config.writeConcern, _self._config.softWriteConcern},
          _commitIndex, _inMemoryLog.getLastTermIndexPair());
  _lastCommitFailReason = commitFailReason;

  LOG_CTX("6a6c0", TRACE, _self._logContext)
      << "calculated commit index as " << newCommitIndex
      << ", current commit index = " << _commitIndex;
  TRI_ASSERT(newCommitIndex >= _commitIndex);
  if (newCommitIndex > _commitIndex) {
    auto const quorum_data = std::make_shared<QuorumData>(
        newCommitIndex, _self._currentTerm, std::move(quorum));
    return updateCommitIndexLeader(newCommitIndex, quorum_data);
  }
  return {};
}

auto replicated_log::LogLeader::GuardedLeaderData::getLocalStatistics() const
    -> LogStatistics {
  auto result = LogStatistics{};
  result.commitIndex = _commitIndex;
  result.firstIndex = _inMemoryLog.getFirstIndex();
  result.spearHead = _inMemoryLog.getLastTermIndexPair();
  return result;
}

replicated_log::LogLeader::GuardedLeaderData::GuardedLeaderData(
    replicated_log::LogLeader& self, InMemoryLog inMemoryLog)
    : _self(self), _inMemoryLog(std::move(inMemoryLog)) {}

auto replicated_log::LogLeader::release(LogIndex doneWithIdx) -> Result {
  return _guardedLeaderData.doUnderLock([&](GuardedLeaderData& self) -> Result {
    TRI_ASSERT(doneWithIdx <= self._inMemoryLog.getLastIndex());
    if (doneWithIdx <= self._releaseIndex) {
      return {};
    }
    self._releaseIndex = doneWithIdx;
    LOG_CTX("a0c96", TRACE, _logContext)
        << "new release index set to " << self._releaseIndex;
    return self.checkCompaction();
  });
}

auto replicated_log::LogLeader::GuardedLeaderData::checkCompaction() -> Result {
  auto const compactionStop = std::min(_largestCommonIndex, _releaseIndex + 1);
  LOG_CTX("080d6", TRACE, _self._logContext)
      << "compaction index calculated as " << compactionStop;
  if (compactionStop <= _inMemoryLog.getFirstIndex() + 1000) {
    // only do a compaction every 1000 entries
    LOG_CTX("ebba0", TRACE, _self._logContext)
        << "won't trigger a compaction, not enough entries. First index = "
        << _inMemoryLog.getFirstIndex();
    return {};
  }

  auto newLog = _inMemoryLog.release(compactionStop);
  auto res = _self._localFollower->release(compactionStop);
  if (res.ok()) {
    _inMemoryLog = std::move(newLog);
  }
  LOG_CTX("f1029", TRACE, _self._logContext)
      << "compaction result = " << res.errorMessage();
  return res;
}

auto replicated_log::LogLeader::GuardedLeaderData::calculateCommitLag()
    const noexcept -> std::chrono::duration<double, std::milli> {
  auto memtry = _inMemoryLog.getEntryByIndex(_commitIndex + 1);
  if (memtry.has_value()) {
    return std::chrono::duration_cast<
        std::chrono::duration<double, std::milli>>(
        std::chrono::steady_clock::now() - memtry->insertTp());
  } else {
    TRI_ASSERT(_commitIndex == _inMemoryLog.getLastIndex())
        << "If there is no entry following the commitIndex the last index "
           "should be the commitIndex. _commitIndex = "
        << _commitIndex << ", lastIndex = " << _inMemoryLog.getLastIndex();
    return {};
  }
}

auto replicated_log::LogLeader::getReplicatedLogSnapshot() const
    -> InMemoryLog::log_type {
  auto [log, commitIndex] =
      _guardedLeaderData.doUnderLock([](auto const& leaderData) {
        if (leaderData._didResign) {
          throw ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
        }

        return std::make_pair(leaderData._inMemoryLog, leaderData._commitIndex);
      });

  return log.takeSnapshotUpToAndIncluding(commitIndex).copyFlexVector();
}

auto replicated_log::LogLeader::waitForIterator(LogIndex index)
    -> replicated_log::ILogParticipant::WaitForIteratorFuture {
  if (index == LogIndex{0}) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid parameter; log index 0 is invalid");
  }

  return waitFor(index).thenValue([this, self = shared_from_this(), index](
                                      auto&& quorum) -> WaitForIteratorFuture {
    auto [actualIndex, iter] = _guardedLeaderData.doUnderLock(
        [index](GuardedLeaderData& leaderData)
            -> std::pair<LogIndex, std::unique_ptr<LogRangeIterator>> {
          TRI_ASSERT(index <= leaderData._commitIndex);

          /*
           * This code here ensures that if only private log entries are present
           * we do not reply with an empty iterator but instead wait for the
           * next entry containing payload.
           */

          auto testIndex = index;
          while (testIndex <= leaderData._commitIndex) {
            auto memtry = leaderData._inMemoryLog.getEntryByIndex(testIndex);
            if (!memtry.has_value()) {
              break;
            }
            if (memtry->entry().logPayload().has_value()) {
              break;
            }
            testIndex = testIndex + 1;
          }

          if (testIndex > leaderData._commitIndex) {
            return std::make_pair(testIndex, nullptr);
          }

          return std::make_pair(testIndex,
                                leaderData.getCommittedLogIterator(testIndex));
        });

    // call here, otherwise we deadlock with waitFor
    if (iter == nullptr) {
      return waitForIterator(actualIndex);
    }

    return std::move(iter);
  });
}

auto replicated_log::LogLeader::copyInMemoryLog() const
    -> replicated_log::InMemoryLog {
  return _guardedLeaderData.getLockedGuard()->_inMemoryLog;
}

replicated_log::LogLeader::LocalFollower::LocalFollower(
    replicated_log::LogLeader& self, LoggerContext logContext,
    std::unique_ptr<LogCore> logCore, [[maybe_unused]] TermIndexPair lastIndex)
    : _leader(self),
      _logContext(std::move(logContext)),
      _guardedLogCore(std::move(logCore)) {
  // TODO save lastIndex. note that it must be protected under the same mutex as
  //      insertions in the persisted log in logCore.
  // TODO use lastIndex in appendEntries to assert that the request matches the
  //      existing log.
  // TODO in maintainer mode only, read here the last entry from logCore, and
  //      assert that lastIndex matches that entry.
}

auto replicated_log::LogLeader::LocalFollower::getParticipantId() const noexcept
    -> ParticipantId const& {
  return _leader.getParticipantId();
}

auto replicated_log::LogLeader::LocalFollower::appendEntries(
    AppendEntriesRequest const request)
    -> futures::Future<AppendEntriesResult> {
  MeasureTimeGuard measureTimeGuard(
      _leader._logMetrics->replicatedLogFollowerAppendEntriesRtUs);

  auto messageLogContext =
      _logContext.with<logContextKeyMessageId>(request.messageId)
          .with<logContextKeyPrevLogIdx>(request.prevLogEntry.index)
          .with<logContextKeyPrevLogTerm>(request.prevLogEntry.term)
          .with<logContextKeyLeaderCommit>(request.leaderCommit);

  auto returnAppendEntriesResult =
      [term = request.leaderTerm, messageId = request.messageId,
       logContext = messageLogContext,
       measureTime = std::move(measureTimeGuard)](Result const& res) mutable {
        // fire here because the lambda is destroyed much later in a future
        measureTime.fire();
        if (!res.ok()) {
          LOG_CTX("fdc87", FATAL, logContext)
              << "local follower failed to write entries: " << res;
          FATAL_ERROR_EXIT();
        }
        LOG_CTX("e0800", TRACE, logContext)
            << "local follower completed append entries";
        return AppendEntriesResult{term, messageId};
      };

  LOG_CTX("6fa8b", TRACE, messageLogContext)
      << "local follower received append entries";

  if (request.entries.empty()) {
    // Nothing to do here, save some work.
    return returnAppendEntriesResult(Result(TRI_ERROR_NO_ERROR));
  }

  auto iter = std::make_unique<InMemoryPersistedLogIterator>(request.entries);
  return _guardedLogCore.doUnderLock([&](auto& logCore)
                                         -> futures::Future<
                                             AppendEntriesResult> {
    if (logCore == nullptr) {
      LOG_CTX("e9b70", DEBUG, messageLogContext)
          << "local follower received append entries although the log core is "
             "moved away.";
      return AppendEntriesResult::withRejection(
          request.leaderTerm, request.messageId,
          {AppendEntriesErrorReason::ErrorType::kLostLogCore});
    }

    // Note that the beginning of iter here is always (and must be) exactly the
    // next index after the last one in the LogCore.
    return logCore->insertAsync(std::move(iter), request.waitForSync)
        .thenValue(std::move(returnAppendEntriesResult));
  });
}

auto replicated_log::LogLeader::LocalFollower::resign() && noexcept
    -> std::unique_ptr<LogCore> {
  LOG_CTX("2062b", TRACE, _logContext)
      << "local follower received resign, term = " << _leader._currentTerm;
  // Although this method is marked noexcept, the doUnderLock acquires a
  // std::mutex which can throw an exception. In that case we just crash here.
  return _guardedLogCore.doUnderLock([&](auto& guardedLogCore) {
    auto logCore = std::move(guardedLogCore);
    LOG_CTX_IF("0f9b8", DEBUG, _logContext, logCore == nullptr)
        << "local follower asked to resign but log core already gone, term = "
        << _leader._currentTerm;
    return logCore;
  });
}

auto replicated_log::LogLeader::isLeadershipEstablished() const noexcept
    -> bool {
  return _guardedLeaderData.getLockedGuard()->_leadershipEstablished;
}

void replicated_log::LogLeader::establishLeadership(
    std::shared_ptr<ParticipantsConfig const> config) {
  LOG_CTX("f3aa8", TRACE, _logContext) << "trying to establish leadership";
  auto waitForIndex =
      _guardedLeaderData.doUnderLock([](GuardedLeaderData& data) {
        auto const lastIndex = data._inMemoryLog.getLastTermIndexPair();
        TRI_ASSERT(lastIndex.term != data._self._currentTerm);
        // Immediately append an empty log entry in the new term. This is
        // necessary because we must not commit entries of older terms, but do
        // not want to wait with committing until the next insert.

        // Also make sure that this entry is written with waitForSync = true to
        // ensure that entries of the previous term are synced as well.
        auto firstIndex = data.insertInternal(std::nullopt, true, std::nullopt);
        TRI_ASSERT(firstIndex == lastIndex.index + 1);
        return firstIndex;
      });

  TRI_ASSERT(waitForIndex == _firstIndexOfCurrentTerm);
  waitFor(waitForIndex)
      .thenFinal([weak = weak_from_this(), config = std::move(config)](
                     futures::Try<WaitForResult>&& result) mutable noexcept {
        if (auto self = weak.lock(); self) {
          try {
            result.throwIfFailed();
            self->_guardedLeaderData.doUnderLock([&](auto& data) {
              data._leadershipEstablished = true;
              if (data.activeParticipantsConfig->generation ==
                  config->generation) {
                data.committedParticipantsConfig = std::move(config);
              }
            });
            LOG_CTX("536f4", TRACE, self->_logContext)
                << "leadership established";
          } catch (ParticipantResignedException const& err) {
            LOG_CTX("22264", TRACE, self->_logContext)
                << "failed to establish leadership due to resign: "
                << err.what();
          } catch (std::exception const& err) {
            LOG_CTX("5ceda", FATAL, self->_logContext)
                << "failed to establish leadership: " << err.what();
          }
        } else {
          LOG_TOPIC("94696", TRACE, Logger::REPLICATION2)
              << "leader is already gone, no leadership was established";
        }
      });
}

auto replicated_log::LogLeader::waitForLeadership()
    -> replicated_log::ILogParticipant::WaitForFuture {
  return waitFor(_firstIndexOfCurrentTerm);
}

auto replicated_log::LogLeader::updateParticipantsConfig(
    std::shared_ptr<ParticipantsConfig const> config,
    std::size_t previousGeneration,
    std::unordered_map<ParticipantId, std::shared_ptr<AbstractFollower>>
        additionalFollowers,
    std::vector<ParticipantId> const& followersToRemove) -> LogIndex {
  LOG_CTX("ac277", TRACE, _logContext)
      << "trying to update configuration to generation " << config->generation;
  TRI_ASSERT(previousGeneration < config->generation);
  auto waitForIndex = _guardedLeaderData.doUnderLock([&](GuardedLeaderData&
                                                             data) {
    if (data.activeParticipantsConfig->generation >= config->generation) {
      auto const message = basics::StringUtils::concatT(
          "updated participant config generation is smaller or equal to "
          "current generation - refusing to update; ",
          "new = ", config->generation,
          ", current = ", data.activeParticipantsConfig->generation);
      LOG_CTX("bab5b", TRACE, _logContext) << message;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
    }
    if (data.activeParticipantsConfig->generation != previousGeneration) {
      // This is to make sure the `additionalFollowers` list is really the
      // (asymmetric) difference between the current and new configuration.
      auto const message = basics::StringUtils::concatT(
          "assumed participant config generation does not match the current "
          "generation - refusing to update; ",
          "previous = ", previousGeneration,
          ", current = ", data.activeParticipantsConfig->generation);
      LOG_CTX("8dc8b", TRACE, _logContext) << message;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // all participants in the new configuration must either exist already, or
    // be added via additionalFollowers.
    {
      auto const& newConfigParticipants = config->participants;
      TRI_ASSERT(std::all_of(newConfigParticipants.begin(),
                             newConfigParticipants.end(), [&](auto const& it) {
                               return data._follower.contains(it.first) ||
                                      additionalFollowers.contains(it.first) ||
                                      it.first == data._self.getParticipantId();
                             }));
    }
#endif

    // Create a copy. This is important to keep the following code
    // exception-safe, in particular never leave data._follower behind in a
    // half-updated state.
    auto followers = data._follower;

    {  // remove obsolete followers
      for (auto const& it : followersToRemove) {
        followers.erase(it);
      }
    }
    {  // add new followers
      for (auto&& [participantId, abstractFollowerPtr] : additionalFollowers) {
        auto const lastIndex =
            data._inMemoryLog.getLastTermIndexPair().index.saturatedDecrement();
        followers.try_emplace(
            participantId,
            std::make_shared<FollowerInfo>(std::move(abstractFollowerPtr),
                                           TermIndexPair{LogTerm{0}, lastIndex},
                                           data._self._logContext));
      }
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // all participants (but the leader) in the new configuration must now be
    // part of followers
    {
      auto const& newConfigParticipants = config->participants;
      TRI_ASSERT(std::all_of(newConfigParticipants.begin(),
                             newConfigParticipants.end(), [&](auto const& it) {
                               return followers.contains(it.first) ||
                                      it.first == data._self.getParticipantId();
                             }));
    }
#endif

    auto const idx = data.insertInternal(std::nullopt, true, std::nullopt);
    data.activeParticipantsConfig = config;
    data._follower.swap(followers);

    return idx;
  });

  triggerAsyncReplication();
  waitFor(waitForIndex)
      .thenFinal([weak = weak_from_this(),
                  config](futures::Try<WaitForResult>&& result) noexcept {
        if (auto self = weak.lock(); self) {
          try {
            result.throwIfFailed();
            if (auto guard = self->_guardedLeaderData.getLockedGuard();
                guard->activeParticipantsConfig->generation ==
                config->generation) {
              // Make sure config is the currently active configuration. It
              // could happen that activeParticipantsConfig was changed before
              // config got any chance to see anything committed, thus never
              // being considered an actual committedParticipantsConfig. In this
              // case we skip it.
              guard->committedParticipantsConfig = config;
              LOG_CTX("536f5", DEBUG, self->_logContext)
                  << "configuration committed, generation "
                  << config->generation;
            } else {
              LOG_CTX("fd245", TRACE, self->_logContext)
                  << "configuration already newer than generation "
                  << config->generation;
            }
          } catch (ParticipantResignedException const& err) {
            LOG_CTX("3959f", DEBUG, self->_logContext)
                << "leader resigned before new participant configuration was "
                   "committed: "
                << err.message();
          } catch (std::exception const& err) {
            LOG_CTX("1af0f", FATAL, self->_logContext)
                << "failed to commit new participant config; " << err.what();
            FATAL_ERROR_EXIT();  // TODO is there nothing we can do here?
          }
        }

        LOG_TOPIC("a4fc1", TRACE, Logger::REPLICATION2)
            << "leader is already gone, configuration change was not committed";
      });

  return waitForIndex;
}

auto replicated_log::LogLeader::getCommitIndex() const noexcept -> LogIndex {
  return _guardedLeaderData.getLockedGuard()->_commitIndex;
}

auto replicated_log::LogLeader::getParticipantConfigGenerations() const noexcept
    -> std::pair<std::size_t, std::optional<std::size_t>> {
  return _guardedLeaderData.doUnderLock([&](GuardedLeaderData const& data) {
    auto activeGeneration = data.activeParticipantsConfig->generation;
    auto committedGeneration = std::optional<std::size_t>{};

    if (auto committedConfig = data.committedParticipantsConfig;
        committedConfig != nullptr) {
      committedGeneration = committedConfig->generation;
    }

    return std::make_pair(activeGeneration, committedGeneration);
  });
}

auto replicated_log::LogLeader::LocalFollower::release(LogIndex stop) const
    -> Result {
  auto res = _guardedLogCore.doUnderLock([&](auto& core) {
    LOG_CTX("23745", DEBUG, _logContext)
        << "local follower releasing with stop at " << stop;
    return core->removeFront(stop).get();
  });
  LOG_CTX_IF("2aba1", WARN, _logContext, res.fail())
      << "local follower failed to release log entries: " << res.errorMessage();
  return res;
}

replicated_log::LogLeader::PreparedAppendEntryRequest::
    PreparedAppendEntryRequest(
        std::shared_ptr<LogLeader> const& logLeader,
        std::shared_ptr<FollowerInfo> follower,
        std::chrono::steady_clock::duration executionDelay)
    : _parentLog(logLeader),
      _follower(std::move(follower)),
      _executionDelay(executionDelay) {}

replicated_log::LogLeader::FollowerInfo::FollowerInfo(
    std::shared_ptr<AbstractFollower> impl, TermIndexPair lastLogIndex,
    LoggerContext const& logContext)
    : _impl(std::move(impl)),
      lastAckedEntry(lastLogIndex),
      logContext(
          logContext.with<logContextKeyLogComponent>("follower-info")
              .with<logContextKeyFollowerId>(_impl->getParticipantId())) {}
