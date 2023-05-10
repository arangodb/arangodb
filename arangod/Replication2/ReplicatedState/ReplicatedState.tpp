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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ReplicatedState.h"

#include <string>
#include <unordered_map>
#include <utility>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/DownCast.h"
#include "Logger/LogContextKeys.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/LazyDeserializingIterator.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
ReplicatedStateManager<S>::ReplicatedStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::unique_ptr<CoreType> logCore, std::shared_ptr<Factory> factory,
    std::shared_ptr<IScheduler> scheduler)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _factory(std::move(factory)),
      _scheduler(std::move(scheduler)),
      _guarded{std::make_shared<UnconfiguredStateManager<S>>(
          _loggerContext.with<logContextKeyStateRole>(
              static_strings::StringUnconfigured),
          std::move(logCore))} {}

template<typename S>
void ReplicatedStateManager<S>::acquireSnapshot(ServerID leader,
                                                LogIndex commitIndex,
                                                std::uint64_t version) {
  auto guard = _guarded.getLockedGuard();

  std::visit(overload{
                 [&](std::shared_ptr<FollowerStateManager<S>>& manager) {
                   LOG_CTX("52a11", DEBUG, _loggerContext)
                       << "try to acquire a new snapshot, starting at "
                       << commitIndex;
                   manager->acquireSnapshot(leader, commitIndex, version);
                 },
                 [](auto&) {
                   ADB_PROD_ASSERT(false)
                       << "State is not a follower (or uninitialized), but "
                          "acquireSnapshot is called";
                 },
             },
             guard->_currentManager);
}

template<typename S>
void ReplicatedStateManager<S>::updateCommitIndex(LogIndex index) {
  auto guard = _guarded.getLockedGuard();

  std::visit(overload{
                 [index](auto& manager) { manager->updateCommitIndex(index); },
                 [](std::shared_ptr<UnconfiguredStateManager<S>>& manager) {
                   ADB_PROD_ASSERT(false) << "update commit index called on "
                                             "an unconfigured state manager";
                 },
             },
             guard->_currentManager);
}

template<typename S>
auto ReplicatedStateManager<S>::resignCurrentState() noexcept
    -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> {
  auto guard = _guarded.getLockedGuard();
  auto&& [core, methods] =
      std::visit([](auto& manager) { return std::move(*manager).resign(); },
                 guard->_currentManager);
  // TODO Is it allowed to happen that resign() is called on an unconfigured
  //      state?
  ADB_PROD_ASSERT(
      std::holds_alternative<std::shared_ptr<UnconfiguredStateManager<S>>>(
          guard->_currentManager) == (methods == nullptr));
  guard->_currentManager
      .template emplace<std::shared_ptr<UnconfiguredStateManager<S>>>(
          std::make_shared<UnconfiguredStateManager<S>>(
              _loggerContext.template with<logContextKeyStateRole>(
                  static_strings::StringUnconfigured),
              std::move(core)));
  return std::move(methods);
}

template<typename S>
void ReplicatedStateManager<S>::leadershipEstablished(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods) {
  auto guard = _guarded.getLockedGuard();
  ADB_PROD_ASSERT(
      std::holds_alternative<std::shared_ptr<UnconfiguredStateManager<S>>>(
          guard->_currentManager));
  auto&& [core, oldMethods] =
      std::move(*std::get<std::shared_ptr<UnconfiguredStateManager<S>>>(
                    guard->_currentManager))
          .resign();
  ADB_PROD_ASSERT(oldMethods == nullptr);
  auto stream = std::make_shared<typename LeaderStateManager<S>::StreamImpl>(
      std::move(methods));
  auto leaderState = _factory->constructLeader(std::move(core));
  // TODO Pass the stream during construction already, and delete the
  //      "setStream" method; after that, the leader state implementation can
  //      also really rely on the stream being there.
  leaderState->setStream(stream);
  auto& manager =
      guard->_currentManager
          .template emplace<std::shared_ptr<LeaderStateManager<S>>>(
              std::make_shared<LeaderStateManager<S>>(
                  _loggerContext.template with<logContextKeyStateRole>(
                      static_strings::StringLeader),
                  _metrics, std::move(leaderState), std::move(stream)));

  // must be posted on the scheduler to avoid deadlocks with the log
  _scheduler->queue([weak = manager->weak_from_this()]() mutable {
    if (auto manager = weak.lock(); manager != nullptr) {
      manager->recoverEntries();
    }
  });
}

template<typename S>
void ReplicatedStateManager<S>::becomeFollower(
    std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> methods) {
  auto guard = _guarded.getLockedGuard();
  ADB_PROD_ASSERT(
      std::holds_alternative<std::shared_ptr<UnconfiguredStateManager<S>>>(
          guard->_currentManager));
  auto&& [core, oldMethods] =
      std::move(*std::get<std::shared_ptr<UnconfiguredStateManager<S>>>(
                    guard->_currentManager))
          .resign();
  ADB_PROD_ASSERT(oldMethods == nullptr);

  auto followerState = _factory->constructFollower(std::move(core));
  auto stream = std::make_shared<typename FollowerStateManager<S>::StreamImpl>(
      std::move(methods));
  followerState->setStream(stream);
  auto stateManager = std::make_shared<FollowerStateManager<S>>(
      _loggerContext.template with<logContextKeyStateRole>(
          static_strings::StringFollower),
      _metrics, followerState, std::move(stream), _scheduler);
  followerState->setStateManager(stateManager);
  guard->_currentManager
      .template emplace<std::shared_ptr<FollowerStateManager<S>>>(
          std::move(stateManager));
}

template<typename S>
auto ReplicatedStateManager<S>::getInternalStatus() const -> Status {
  auto manager = _guarded.getLockedGuard()->_currentManager;
  auto status = std::visit(overload{[](auto const& manager) -> Status {
                             return {manager->getInternalStatus()};
                           }},
                           manager);

  return status;
}

template<typename S>
auto ReplicatedStateManager<S>::getFollower() const
    -> std::shared_ptr<IReplicatedFollowerStateBase> {
  auto guard = _guarded.getLockedGuard();
  return std::visit(
      overload{
          [](std::shared_ptr<FollowerStateManager<S>> const& manager) {
            return basics::downCast<IReplicatedFollowerStateBase>(
                manager->getStateMachine());
          },
          [](auto const&) -> std::shared_ptr<IReplicatedFollowerStateBase> {
            return nullptr;
          }},
      guard->_currentManager);
}

template<typename S>
auto ReplicatedStateManager<S>::getLeader() const
    -> std::shared_ptr<IReplicatedLeaderStateBase> {
  auto guard = _guarded.getLockedGuard();
  return std::visit(
      overload{[](std::shared_ptr<LeaderStateManager<S>> const& manager) {
                 return basics::downCast<IReplicatedLeaderStateBase>(
                     manager->getStateMachine());
               },
               [](auto const&) -> std::shared_ptr<IReplicatedLeaderStateBase> {
                 return nullptr;
               }},
      guard->_currentManager);
}

template<typename S>
auto ReplicatedStateManager<S>::resign() && -> std::unique_ptr<CoreType> {
  auto guard = _guarded.getLockedGuard();
  auto [core, methods] =
      std::visit([](auto&& mgr) { return std::move(*mgr).resign(); },
                 guard->_currentManager);
  // we should be unconfigured already
  TRI_ASSERT(methods == nullptr);
  // cppcheck-suppress returnStdMoveLocal ; core is not a local variable
  return std::move(core);
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
StreamProxy<S, Interface, ILogMethodsT>::StreamProxy(
    std::unique_ptr<ILogMethodsT> methods)
    : _logMethods(std::move(methods)) {}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::methods() -> MethodsGuard {
  return MethodsGuard{this->_logMethods.getLockedGuard()};
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface,
                 ILogMethodsT>::resign() && -> std::unique_ptr<ILogMethodsT> {
  // cppcheck-suppress returnStdMoveLocal ; bogus
  return std::move(this->_logMethods.getLockedGuard().get());
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::isResigned() const noexcept
    -> bool {
  return _logMethods.getLockedGuard().get() == nullptr;
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::waitFor(LogIndex index)
    -> futures::Future<WaitForResult> {
  // TODO We might want to remove waitFor here, in favor of updateCommitIndex.
  //      It's currently used by DocumentLeaderState::replicateOperation.
  //      If this is removed, delete it also in streams::Stream.
  auto guard = _logMethods.getLockedGuard();
  return guard.get()->waitFor(index).thenValue(
      [](auto const&) { return WaitForResult(); });
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::waitForIterator(LogIndex index)
    -> futures::Future<std::unique_ptr<Iterator>> {
  // TODO As far as I can tell right now, we can get rid of this, but for the
  //      PrototypeState (currently). So:
  //      Delete this, also in streams::Stream.
  auto guard = _logMethods.getLockedGuard();
  return guard.get()->waitForIterator(index).thenValue([](auto&& logIter) {
    std::unique_ptr<Iterator> deserializedIter =
        std::make_unique<LazyDeserializingIterator<EntryType, Deserializer>>(
            std::move(logIter));
    return deserializedIter;
  });
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
auto StreamProxy<S, Interface, ILogMethodsT>::release(LogIndex index) -> void {
  auto guard = _logMethods.getLockedGuard();
  if (auto& methods = guard.get(); methods != nullptr) [[likely]] {
    methods->releaseIndex(index);
  } else {
    throwResignedException();
  }
}

template<typename S, template<typename> typename Interface,
         ValidStreamLogMethods ILogMethodsT>
void StreamProxy<S, Interface, ILogMethodsT>::throwResignedException() {
  static constexpr auto errorCode = ([]() consteval->ErrorCode {
    if constexpr (std::is_same_v<ILogMethodsT,
                                 replicated_log::IReplicatedLogLeaderMethods>) {
      return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
    } else if constexpr (std::is_same_v<
                             ILogMethodsT,
                             replicated_log::IReplicatedLogFollowerMethods>) {
      return TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED;
    } else {
      []<bool flag = false>() {
        static_assert(flag,
                      "Unhandled log methods. This should have been "
                      "prevented by the concept ValidStreamLogMethods.");
      }
      ();
      ADB_PROD_ASSERT(false);
    }
  })();

  // The DocumentStates do not synchronize calls to `release()` or `insert()` on
  // the stream with resigning (see
  // https://github.com/arangodb/arangodb/pull/17850).
  // It relies on the stream throwing an exception in that case.
  throw replicated_log::ParticipantResignedException(errorCode, ADB_HERE);
}

template<typename S>
ProducerStreamProxy<S>::ProducerStreamProxy(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods)
    : StreamProxy<S, streams::ProducerStream,
                  replicated_log::IReplicatedLogLeaderMethods>(
          std::move(methods)) {
  // this produces a lock inversion
  // ADB_PROD_ASSERT(this->_logMethods.getLockedGuard().get() != nullptr);
}

template<typename S>
auto ProducerStreamProxy<S>::insert(const EntryType& v) -> LogIndex {
  auto guard = this->_logMethods.getLockedGuard();
  if (auto& methods = guard.get(); methods != nullptr) [[likely]] {
    return methods->insert(serialize(v));
  } else {
    this->throwResignedException();
  }
}

template<typename S>
auto ProducerStreamProxy<S>::insertDeferred(const EntryType& v)
    -> std::pair<LogIndex, DeferredAction> {
  // TODO Remove this method, it should be superfluous
  auto guard = this->_logMethods.getLockedGuard();
  if (auto& methods = guard.get(); methods != nullptr) [[likely]] {
    return methods->insertDeferred(serialize(v));
  } else {
    this->throwResignedException();
  }
}

template<typename S>
auto ProducerStreamProxy<S>::serialize(const EntryType& v) -> LogPayload {
  auto builder = velocypack::Builder();
  std::invoke(Serializer{}, streams::serializer_tag<EntryType>, v, builder);
  // TODO avoid the copy
  auto payload = LogPayload::createFromSlice(builder.slice());
  return payload;
}

template<typename S>
void LeaderStateManager<S>::recoverEntries() {
  LOG_CTX("1b3d0", DEBUG, _loggerContext) << "starting recovery";
  auto future = _guardedData.getLockedGuard()->recoverEntries();
  std::move(future).thenFinal(
      [weak = this->weak_from_this()](futures::Try<Result>&& tryResult) {
        // TODO error handling
        ADB_PROD_ASSERT(tryResult.hasValue());
        ADB_PROD_ASSERT(tryResult.get().ok()) << tryResult.get();
        if (auto self = weak.lock(); self != nullptr) {
          auto guard = self->_guardedData.getLockedGuard();
          guard->_leaderState->onRecoveryCompleted();
          guard->_recoveryCompleted = true;
          LOG_CTX("1b3de", INFO, self->_loggerContext) << "recovery completed";
        }
      });
}

template<typename S>
auto LeaderStateManager<S>::GuardedData::recoverEntries() {
  auto logIter = _stream->methods()->getCommittedLogIterator();
  auto deserializedIter =
      std::make_unique<LazyDeserializingIterator<EntryType, Deserializer>>(
          std::move(logIter));
  MeasureTimeGuard timeGuard(*_metrics.replicatedStateRecoverEntriesRtt);
  // cppcheck-suppress accessMoved ; deserializedIter is only accessed once
  auto fut = _leaderState->recoverEntries(std::move(deserializedIter))
                 .then([guard = std::move(timeGuard)](auto&& res) mutable {
                   guard.fire();
                   return std::move(res.get());
                 });
  return fut;
}

template<typename S>
LeaderStateManager<S>::LeaderStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IReplicatedLeaderState<S>> leaderState,
    std::shared_ptr<StreamImpl> stream)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _guardedData{_loggerContext, *_metrics, std::move(leaderState),
                   std::move(stream)} {}

template<typename S>
auto LeaderStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  return std::move(_guardedData.getLockedGuard().get()).resign();
}

template<typename S>
auto LeaderStateManager<S>::getInternalStatus() const -> Status::Leader {
  auto guard = _guardedData.getLockedGuard();
  if (guard->_leaderState == nullptr) {
    // we have already resigned
    return {Status::Leader::Resigned{}};
  } else if (guard->_recoveryCompleted) {
    return {Status::Leader::Operational{}};
  } else {
    return {Status::Leader::InRecovery{}};
  }
}

template<typename S>
auto LeaderStateManager<S>::getStateMachine() const
    -> std::shared_ptr<IReplicatedLeaderState<S>> {
  auto guard = _guardedData.getLockedGuard();
  if (guard->_recoveryCompleted) {
    return guard->_leaderState;
  } else {
    return nullptr;
  }
}

template<typename S>
auto LeaderStateManager<S>::GuardedData::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  auto core = std::move(*_leaderState).resign();
  // resign the stream after the state, so the state won't try to use the
  // resigned stream.
  auto methods = std::move(*_stream).resign();
  _leaderState = nullptr;
  return {std::move(core), std::move(methods)};
}

template<typename S>
void FollowerStateManager<S>::updateCommitIndex(LogIndex commitIndex) {
  auto maybeFuture = _guardedData.getLockedGuard()->updateCommitIndex(
      commitIndex, _metrics, _scheduler);
  // note that we release the lock before calling "then"

  // we get a future iff applyEntries was called
  if (maybeFuture.has_value()) {
    auto& future = *maybeFuture;
    std::move(future).thenFinal(
        [weak = this->weak_from_this()](auto&& tryResult) {
          if (auto self = weak.lock(); self != nullptr) {
            auto res = basics::catchToResult([&] {
              return std::forward<decltype(tryResult)>(tryResult).get();
            });
            self->handleApplyEntriesResult(res);
          }
        });
  }
}

template<typename S>
void FollowerStateManager<S>::handleApplyEntriesResult(arangodb::Result res) {
  auto maybeFuture = [&]() -> std::optional<futures::Future<Result>> {
    auto guard = _guardedData.getLockedGuard();
    if (res.ok()) {
      ADB_PROD_ASSERT(guard->_applyEntriesIndexInFlight.has_value());
      _metrics->replicatedStateApplyDebt->operator-=(
          guard->_applyEntriesIndexInFlight->value -
          guard->_lastAppliedIndex.value);
      auto const index = guard->_lastAppliedIndex =
          *guard->_applyEntriesIndexInFlight;
      // TODO
      //   _metrics->replicatedStateNumberProcessedEntries->count(range.count());

      auto queue = guard->getResolvablePromises(index);
      queue.resolveAllWith(futures::Try(index), [&scheduler =
                                                     *_scheduler]<typename F>(
                                                    F&& f) noexcept {
        static_assert(noexcept(std::decay_t<decltype(f)>(std::forward<F>(f))));
        scheduler.queue([f = std::forward<F>(f)]() mutable noexcept {
          static_assert(noexcept(std::forward<F>(f)()));
          std::forward<F>(f)();
        });
      });
    }
    guard->_applyEntriesIndexInFlight = std::nullopt;

    if (res.fail()) {
      _metrics->replicatedStateNumberApplyEntriesErrors->operator++();
      switch (static_cast<int>(res.errorNumber())) {
        case static_cast<int>(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED): {
          // Log follower has resigned, we'll be resigned as well. We just stop
          // working.
          return std::nullopt;
        }
      }
    }

    ADB_PROD_ASSERT(!res.fail())
        << _loggerContext
        << " Unexpected error returned by apply entries: " << res;

    if (res.fail() || guard->_commitIndex > guard->_lastAppliedIndex) {
      return guard->maybeScheduleApplyEntries(_metrics, _scheduler);
    }
    return std::nullopt;
  }();
  if (maybeFuture) {
    std::move(*maybeFuture)
        .thenFinal([weak = this->weak_from_this()](auto&& tryResult) {
          if (auto self = weak.lock(); self != nullptr) {
            auto res = basics::catchToResult([&] {
              return std::forward<decltype(tryResult)>(tryResult).get();
            });
            self->handleApplyEntriesResult(res);
          }
        });
  }
}

template<typename S>
auto FollowerStateManager<S>::GuardedData::updateCommitIndex(
    LogIndex commitIndex,
    std::shared_ptr<ReplicatedStateMetrics> const& metrics,
    std::shared_ptr<IScheduler> const& scheduler)
    -> std::optional<futures::Future<Result>> {
  LOG_DEVEL_IF(false) << "updating commit index from " << _commitIndex << " to "
                      << commitIndex;
  if (_stream == nullptr) {
    return std::nullopt;
  }
  ADB_PROD_ASSERT(commitIndex > _commitIndex);
  metrics->replicatedStateApplyDebt->operator+=(commitIndex.value -
                                                _commitIndex.value);
  _commitIndex = std::max(_commitIndex, commitIndex);
  return maybeScheduleApplyEntries(metrics, scheduler);
}

template<typename S>
auto FollowerStateManager<S>::GuardedData::maybeScheduleApplyEntries(
    std::shared_ptr<ReplicatedStateMetrics> const& metrics,
    std::shared_ptr<IScheduler> const& scheduler)
    -> std::optional<futures::Future<Result>> {
  if (_stream == nullptr) {
    return std::nullopt;
  }
  if (_commitIndex > _lastAppliedIndex and
      not _applyEntriesIndexInFlight.has_value()) {
    // Apply at most 1000 entries at once, so we have a smoother progression.
    _applyEntriesIndexInFlight =
        std::min(_commitIndex, _lastAppliedIndex + 1000);
    auto range =
        LogRange{_lastAppliedIndex + 1, *_applyEntriesIndexInFlight + 1};
    auto promise = futures::Promise<Result>();
    auto future = promise.getFuture();
    auto rttGuard = MeasureTimeGuard(*metrics->replicatedStateApplyEntriesRtt);
    // As applyEntries is currently synchronous, we have to post it on the
    // scheduler to avoid blocking the current appendEntries request from
    // returning. By using _applyEntriesIndexInFlight we make sure not to call
    // it multiple time in parallel.
    scheduler->queue([promise = std::move(promise), stream = _stream, range,
                      followerState = _followerState,
                      rttGuard = std::move(rttGuard)]() mutable {
      using IterType =
          LazyDeserializingIterator<EntryType const&, Deserializer>;
      auto iter = [&]() -> std::unique_ptr<IterType> {
        auto methods = stream->methods();
        if (methods.isResigned()) {
          return nullptr;  // Nothing to do, follower already resigned
        }
        // get an iterator for the range [last_applied + 1, commitIndex + 1)
        auto logIter = methods->getCommittedLogIterator(range);
        return std::make_unique<IterType>(std::move(logIter));
      }();
      if (iter != nullptr) {
        // cppcheck-suppress accessMoved ; iter is accessed only once - bogus
        followerState->applyEntries(std::move(iter))
            .thenFinal(
                [promise = std::move(promise),
                 rttGuard = std::move(rttGuard)](auto&& tryResult) mutable {
                  rttGuard.fire();
                  promise.setTry(std::move(tryResult));
                });
      }
    });

    return future;
  } else {
    return std::nullopt;
  }
}

template<typename S>
auto FollowerStateManager<S>::GuardedData::getResolvablePromises(
    LogIndex index) noexcept -> WaitForQueue {
  return _waitQueue.splitLowerThan(index + 1);
}

template<typename S>
auto FollowerStateManager<S>::GuardedData::waitForApplied(LogIndex index)
    -> WaitForQueue::WaitForFuture {
  if (index <= _lastAppliedIndex) {
    // Resolve the promise immediately before returning the future
    auto promise = WaitForQueue::WaitForPromise();
    promise.setTry(futures::Try(_lastAppliedIndex));
    return promise.getFuture();
  }
  return _waitQueue.waitFor(index);
}

template<typename S>
FollowerStateManager<S>::FollowerStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IReplicatedFollowerState<S>> followerState,
    std::shared_ptr<StreamImpl> stream, std::shared_ptr<IScheduler> scheduler)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _scheduler(std::move(scheduler)),
      _guardedData{std::move(followerState), std::move(stream)} {}

namespace {
inline auto calcRetryDuration(std::uint64_t retryCount)
    -> std::chrono::steady_clock::duration {
  // Capped exponential backoff. Wait for 100us, 200us, 400us, ...
  // until at most 100us * 2 ** 17 == 13.11s.
  auto executionDelay = std::chrono::microseconds{100} *
                        (1u << std::min(retryCount, std::uint64_t{17}));
  return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      executionDelay);
}
}  // namespace

template<typename S>
auto FollowerStateManager<S>::backOffSnapshotRetry()
    -> futures::Future<futures::Unit> {
  constexpr static auto countSuffix = [](auto count) {
    switch (count) {
      case 1:
        return "st";
      case 2:
        return "nd";
      case 3:
        return "rd";
      default:
        return "th";
    }
  };
  constexpr static auto fmtTime = [](auto duration) {
    using namespace std::chrono_literals;
    using namespace std::chrono;
    if (duration < 10us) {
      return fmt::format("{}ns", duration_cast<nanoseconds>(duration).count());
    } else if (duration < 10ms) {
      return fmt::format("{}us", duration_cast<microseconds>(duration).count());
    } else if (duration < 10s) {
      return fmt::format("{}ms", duration_cast<milliseconds>(duration).count());
    } else if (duration < 10min) {
      return fmt::format("{}s", duration_cast<seconds>(duration).count());
    } else {
      return fmt::format("{}min", duration_cast<minutes>(duration).count());
    }
  };

  auto const retryCount =
      this->_guardedData.getLockedGuard()->_snapshotErrorCounter;
  auto const duration = calcRetryDuration(retryCount);
  LOG_CTX("2ea59", TRACE, _loggerContext)
      << "retry snapshot transfer after " << fmtTime(duration) << ", "
      << retryCount << countSuffix(retryCount) << " retry";
  return _scheduler->delayedFuture(duration,
                                   "replication2 retry snapshot transfer");
}

template<typename S>
void FollowerStateManager<S>::registerSnapshotError(Result error) noexcept {
  _guardedData.getLockedGuard()->registerSnapshotError(
      std::move(error), *_metrics->replicatedStateNumberAcquireSnapshotErrors);
}

template<typename S>
void FollowerStateManager<S>::GuardedData::registerSnapshotError(
    Result error, metrics::Counter& counter) noexcept {
  TRI_ASSERT(error.fail());
  _lastSnapshotError = std::move(error);
  _snapshotErrorCounter += 1;
  counter.operator++();
}

template<typename S>
void FollowerStateManager<S>::GuardedData::clearSnapshotErrors() noexcept {
  _lastSnapshotError = std::nullopt;
  _snapshotErrorCounter = 0;
}

template<typename S>
void FollowerStateManager<S>::acquireSnapshot(ServerID leader, LogIndex index,
                                              std::uint64_t version) {
  LOG_CTX("c4d6b", DEBUG, _loggerContext) << "calling acquire snapshot";
  MeasureTimeGuard rttGuard(*_metrics->replicatedStateAcquireSnapshotRtt);
  GaugeScopedCounter snapshotCounter(
      *_metrics->replicatedStateNumberWaitingForSnapshot);
  auto fut = _guardedData.doUnderLock(
      [&](auto& self) { return self._followerState->acquireSnapshot(leader); });
  // note that we release the lock before calling "then" to avoid deadlocks

  // must be posted on the scheduler to avoid deadlocks with the log
  _scheduler->queue([weak = this->weak_from_this(), fut = std::move(fut),
                     rttGuard = std::move(rttGuard),
                     snapshotCounter = std::move(snapshotCounter),
                     leader = std::move(leader), index, version]() mutable {
    std::move(fut).thenFinal([weak = std::move(weak),
                              rttGuard = std::move(rttGuard),
                              snapshotCounter = std::move(snapshotCounter),
                              leader = std::move(leader), index,
                              version](futures::Try<Result>&&
                                           tryResult) mutable noexcept {
      rttGuard.fire();
      snapshotCounter.fire();
      if (auto self = weak.lock(); self != nullptr) {
        LOG_CTX("13f07", TRACE, self->_loggerContext)
            << "acquire snapshot returned";
        auto result = basics::catchToResult([&] { return tryResult.get(); });
        if (result.ok()) {
          LOG_CTX("44d58", DEBUG, self->_loggerContext)
              << "snapshot transfer successfully completed, informing "
                 "replicated log";
          auto guard = self->_guardedData.getLockedGuard();
          guard->clearSnapshotErrors();
          auto res =
              basics::downCast<replicated_log::IReplicatedLogFollowerMethods>(
                  *guard->_stream->methods())
                  .snapshotCompleted(version);
          // TODO (How) can we handle this more gracefully?
          ADB_PROD_ASSERT(res.ok());
        } else {
          LOG_CTX("9a68a", INFO, self->_loggerContext)
              << "failed to transfer snapshot: " << result
              << " - retry scheduled";
          self->registerSnapshotError(std::move(result));
          self->backOffSnapshotRetry().thenFinal(
              [weak = std::move(weak), leader = std::move(leader), index,
               version](
                  futures::Try<futures::Unit>&& x) mutable noexcept -> void {
                auto const result = basics::catchVoidToResult([&] { x.get(); });
                ADB_PROD_ASSERT(result.ok())
                    << "Unexpected error when backing off snapshot retry: "
                    << result;
                if (auto self = weak.lock(); self != nullptr) {
                  self->acquireSnapshot(std::move(leader), index, version);
                }
              });
        }
      }
    });
  });
}

template<typename S>
auto FollowerStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  auto guard = _guardedData.getLockedGuard();
  auto core = std::move(*guard->_followerState).resign();
  auto methods = std::move(*guard->_stream).resign();
  guard->_followerState = nullptr;
  guard->_stream.reset();
  auto tryResult = futures::Try<LogIndex>(
      std::make_exception_ptr(replicated_log::ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE)));
  guard->_waitQueue.resolveAllWith(
      std::move(tryResult), [&scheduler = *_scheduler]<typename F>(F&& f) {
        static_assert(noexcept(std::decay_t<decltype(f)>(std::forward<F>(f))));
        scheduler.queue([f = std::forward<F>(f)]() mutable noexcept {
          static_assert(noexcept(std::forward<F>(f)()));
          std::forward<F>(f)();
        });
      });

  return {std::move(core), std::move(methods)};
}

template<typename S>
auto FollowerStateManager<S>::getInternalStatus() const -> Status::Follower {
  auto guard = _guardedData.getLockedGuard();
  // TODO maybe this logic should be moved into the replicated log instead, it
  //      seems to contain all the information.
  if (guard->_followerState == nullptr || guard->_stream->isResigned()) {
    return {Status::Follower::Resigned{}};
  } else {
    return {Status::Follower::Constructed{}};
  }
}

template<typename S>
auto FollowerStateManager<S>::getStateMachine() const
    -> std::shared_ptr<IReplicatedFollowerState<S>> {
  return _guardedData.doUnderLock(
      [](auto& data) -> std::shared_ptr<IReplicatedFollowerState<S>> {
        auto& stream = *data._stream;

        // A follower is established if it
        //  a) has a snapshot, and
        //  b) knows the snapshot won't be invalidated in the current term.
        bool const followerEstablished = std::invoke([&] {
          auto methodsGuard = stream.methods();
          return methodsGuard.isResigned() or
                 (methodsGuard->leaderConnectionEstablished() and
                  methodsGuard->checkSnapshotState() ==
                      replicated_log::SnapshotState::AVAILABLE);
        });
        // It is essential that, in the lines above this comment, the snapshot
        // state is checked *after* the leader connection to prevent races. Note
        // that a log truncate will set the snapshot to missing. After a
        // successful append entries, the log won't be truncated again -- during
        // the current term at least. So the snapshot state can toggle from
        // AVAILABLE to MISSING and back to AVAILABLE, but only once; and the
        // commit index will be updated only after the (possible) switch from
        // AVAILABLE to MISSING.

        // Disallow access unless we have a snapshot and are sure the log
        // won't be truncated (and thus the snapshot invalidated) in this
        // term.
        if (followerEstablished) {
          return data._followerState;
        } else {
          return nullptr;
        }
      });
}

template<typename S>
auto FollowerStateManager<S>::waitForApplied(LogIndex index)
    -> WaitForQueue::WaitForFuture {
  return _guardedData.getLockedGuard()->waitForApplied(index);
}

template<typename S>
UnconfiguredStateManager<S>::UnconfiguredStateManager(
    LoggerContext loggerContext, std::unique_ptr<CoreType> core) noexcept
    : _loggerContext(std::move(loggerContext)),
      _guardedData{GuardedData{._core = std::move(core)}} {}

template<typename S>
auto UnconfiguredStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  auto guard = _guardedData.getLockedGuard();
  return {std::move(guard.get()).resign(), nullptr};
}

template<typename S>
auto UnconfiguredStateManager<S>::getInternalStatus() const
    -> Status::Unconfigured {
  return {};
}

template<typename S>
auto UnconfiguredStateManager<S>::GuardedData::resign() && noexcept
    -> std::unique_ptr<CoreType> {
  return std::move(_core);
}

template<typename S>
auto IReplicatedLeaderState<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> const& {
  ADB_PROD_ASSERT(_stream != nullptr)
      << "Replicated leader state: stream accessed before service was "
         "started.";

  return _stream;
}

template<typename S>
auto IReplicatedFollowerState<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> const& {
  ADB_PROD_ASSERT(_stream != nullptr) << "Replicated follower state: stream "
                                         "accessed before service was started.";

  return _stream;
}

template<typename S>
void IReplicatedFollowerState<S>::setStateManager(
    std::shared_ptr<FollowerStateManager<S>> manager) noexcept {
  _manager = manager;
}

template<typename S>
auto IReplicatedFollowerState<S>::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  if (auto manager = _manager.lock(); manager != nullptr) {
    return manager->waitForApplied(index).thenValue(
        [](LogIndex) { return futures::Unit(); });
  } else {
    WaitForAppliedFuture future(
        std::make_exception_ptr(replicated_log::ParticipantResignedException(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE)));
    return future;
  }
}

template<typename S>
ReplicatedState<S>::ReplicatedState(
    GlobalLogIdentifier gid, std::shared_ptr<replicated_log::ReplicatedLog> log,
    std::shared_ptr<Factory> factory, LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IScheduler> scheduler)
    : factory(std::move(factory)),
      gid(std::move(gid)),
      log(std::move(log)),
      loggerContext(std::move(loggerContext)),
      metrics(std::move(metrics)),
      scheduler(std::move(scheduler)) {
  TRI_ASSERT(this->metrics != nullptr);
  this->metrics->replicatedStateNumber->fetch_add(1);
}

template<typename S>
auto ReplicatedState<S>::getFollower() const -> std::shared_ptr<FollowerType> {
  ADB_PROD_ASSERT(manager.has_value());
  return basics::downCast<FollowerType>(manager->getFollower());
}

template<typename S>
auto ReplicatedState<S>::getLeader() const -> std::shared_ptr<LeaderType> {
  ADB_PROD_ASSERT(manager.has_value());
  return basics::downCast<LeaderType>(manager->getLeader());
}

template<typename S>
ReplicatedState<S>::~ReplicatedState() {
  metrics->replicatedStateNumber->fetch_sub(1);
}

template<typename S>
auto ReplicatedState<S>::buildCore(
    TRI_vocbase_t& vocbase,
    std::optional<velocypack::SharedSlice> const& coreParameter) {
  using CoreParameterType = typename S::CoreParameterType;
  if constexpr (std::is_void_v<CoreParameterType>) {
    return factory->constructCore(vocbase, gid);
  } else {
    if (!coreParameter.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Cannot find core parameter for replicated state with "
                      "ID {}, created in database {}, for {} state",
                      gid.id, gid.database, S::NAME));
    }
    auto params = CoreParameterType{};
    try {
      params =
          velocypack::deserialize<CoreParameterType>(coreParameter->slice());
    } catch (std::exception const& ex) {
      LOG_CTX("63857", ERR, loggerContext)
          << "failed to deserialize core parameters for replicated state "
          << gid << ". " << ex.what() << " json = " << coreParameter->toJson();
      throw;
    }
    return factory->constructCore(vocbase, gid, std::move(params));
  }
}

template<typename S>
void ReplicatedState<S>::drop(
    std::shared_ptr<replicated_log::IReplicatedStateHandle> stateHandle) && {
  ADB_PROD_ASSERT(stateHandle != nullptr);
  ADB_PROD_ASSERT(manager.has_value());
  auto core = std::move(*manager).resign();
  ADB_PROD_ASSERT(core != nullptr);

  using CleanupHandler = typename ReplicatedStateTraits<S>::CleanupHandlerType;
  if constexpr (not std::is_void_v<CleanupHandler>) {
    static_assert(
        std::is_invocable_r_v<std::shared_ptr<CleanupHandler>,
                              decltype(&Factory::constructCleanupHandler),
                              Factory>);
    std::shared_ptr<CleanupHandler> cleanupHandler =
        factory->constructCleanupHandler();
    cleanupHandler->drop(std::move(core));
  }
}

template<typename S>
auto ReplicatedState<S>::createStateHandle(
    TRI_vocbase_t& vocbase,
    std::optional<velocypack::SharedSlice> const& coreParameter)
    -> std::unique_ptr<replicated_log::IReplicatedStateHandle> {
  // TODO Should we make sure not to build the core twice?
  auto core = buildCore(vocbase, coreParameter);
  ADB_PROD_ASSERT(not manager.has_value());
  manager.emplace(loggerContext, metrics, std::move(core), factory, scheduler);

  struct Wrapper : replicated_log::IReplicatedStateHandle {
    explicit Wrapper(ReplicatedStateManager<S>& manager) : manager(manager) {}
    // MSVC chokes on trailing return type notation here
    [[nodiscard]] std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>
    resignCurrentState() noexcept override {
      return manager.resignCurrentState();
    }
    void leadershipEstablished(
        std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> ptr)
        override {
      return manager.leadershipEstablished(std::move(ptr));
    }
    void becomeFollower(
        std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> ptr)
        override {
      return manager.becomeFollower(std::move(ptr));
    }
    void acquireSnapshot(ServerID leader, LogIndex index,
                         std::uint64_t version) override {
      return manager.acquireSnapshot(std::move(leader), index, version);
    }
    void updateCommitIndex(LogIndex index) override {
      return manager.updateCommitIndex(index);
    }

    // MSVC chokes on trailing return type notation here
    [[nodiscard]] Status getInternalStatus() const override {
      return manager.getInternalStatus();
    }

    ReplicatedStateManager<S>& manager;
  };

  return std::make_unique<Wrapper>(*manager);
}

}  // namespace arangodb::replication2::replicated_state
