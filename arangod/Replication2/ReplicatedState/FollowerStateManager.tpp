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

#include "FollowerStateManager.h"

#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/ReplicatedState/LazyDeserializingIterator.h"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/Storage/IteratorPosition.h"

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

namespace arangodb::replication2::replicated_state {

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
          guard->_lastAppliedPosition.index().value);
      guard->_lastAppliedPosition = storage::IteratorPosition::fromLogIndex(
          *guard->_applyEntriesIndexInFlight);
      auto const index = guard->_lastAppliedPosition.index();
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

    if (res.fail() ||
        guard->_commitIndex > guard->_lastAppliedPosition.index()) {
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
  if (_commitIndex > _lastAppliedPosition.index() and
      not _applyEntriesIndexInFlight.has_value()) {
    // Apply at most 1000 entries at once, so we have a smoother progression.
    _applyEntriesIndexInFlight =
        std::min(_commitIndex, _lastAppliedPosition.index() + 1000);
    auto range = LogRange{_lastAppliedPosition.index() + 1,
                          *_applyEntriesIndexInFlight + 1};
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
      } else {
        promise.setException(replicated_log::ParticipantResignedException(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE));
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
  if (index <= _lastAppliedPosition.index()) {
    // Resolve the promise immediately before returning the future
    auto promise = WaitForQueue::WaitForPromise();
    promise.setTry(futures::Try(_lastAppliedPosition.index()));
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

}  // namespace arangodb::replication2::replicated_state
