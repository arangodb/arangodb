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

#include "FollowerStateManager.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/MetricsHelper.h"

#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Basics/Exceptions.h"
#include "Scheduler/SchedulerFeature.h"

#include "Metrics/Counter.h"
#include "Metrics/Histogram.h"

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <chrono>

namespace arangodb::replication2::replicated_state {

template<typename S>
auto FollowerStateManager<S>::waitForApplied(LogIndex idx)
    -> futures::Future<futures::Unit> {
  auto guard = _guardedData.getLockedGuard();
  if (guard->_nextWaitForIndex > idx) {
    return futures::Future<futures::Unit>{std::in_place};
  }

  auto it = guard->waitForAppliedQueue.emplace(idx, WaitForAppliedPromise{});
  auto f = it->second.getFuture();
  TRI_ASSERT(f.valid());
  return f;
}

namespace {
inline auto delayedFuture(std::chrono::steady_clock::duration duration)
    -> futures::Future<futures::Unit> {
  if (SchedulerFeature::SCHEDULER) {
    return SchedulerFeature::SCHEDULER->delay(duration);
  }

  std::this_thread::sleep_for(duration);
  return futures::Future<futures::Unit>{std::in_place};
}

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
void FollowerStateManager<S>::run() noexcept {
  waitForLogFollowerResign();

  static auto constexpr throwResultOnError = [](Result result) {
    if (result.fail()) {
      throw basics::Exception(std::move(result), ADB_HERE);
    }
  };

  auto const handleErrors = [weak = this->weak_from_this()](
                                futures::Try<futures::Unit>&& tryResult) {
    // TODO Instead of capturing "this", should we capture the loggerContext
    // instead?
    if (auto self = weak.lock(); self != nullptr) {
      try {
        [[maybe_unused]] futures::Unit _ = tryResult.get();
      } catch (replicated_log::ParticipantResignedException const&) {
        LOG_CTX("0a0db", DEBUG, self->loggerContext)
            << "Log follower resigned, stopping replicated state machine. Will "
               "be restarted soon.";
        return;
      } catch (basics::Exception const& ex) {
        LOG_CTX("2feb8", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine: "
            << ex.message();
        FATAL_ERROR_EXIT();
      } catch (std::exception const& ex) {
        LOG_CTX("8c611", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine: "
            << ex.what();
        FATAL_ERROR_EXIT();
      } catch (...) {
        LOG_CTX("4d160", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine.";
        FATAL_ERROR_EXIT();
      }
    }
  };

  auto const transitionTo = [this](FollowerInternalState state) {
    return [state, weak = this->weak_from_this()](futures::Unit) {
      if (auto self = weak.lock(); self != nullptr) {
        self->_guardedData.getLockedGuard()->updateInternalState(state);
        self->run();
      }
    };
  };

  // Note that because `transitionWith` locks the self-ptr before calling `fn`,
  // fn itself is allowed to just capture and use `this`.
  auto const transitionWith = [that = this]<typename F>(F&& fn) {
    return [
      weak = that->weak_from_this(), fn = std::forward<F>(fn)
    ]<typename T, typename F1 = F>(T && arg) mutable {
      if (auto self = weak.lock(); self != nullptr) {
        auto const state = std::invoke([&] {
          // Don't pass Unit to make that case more convenient.
          if constexpr (std::is_same_v<std::decay_t<T>, futures::Unit>) {
            return std::forward<F1>(fn)();
          } else {
            return std::forward<F1>(fn)(std::forward<T>(arg));
          }
        });
        self->_guardedData.getLockedGuard()->updateInternalState(state);
        self->run();
      }
    };
  };

  static auto constexpr noop = []() {
    auto promise = futures::Promise<futures::Unit>{};
    promise.setValue(futures::Unit());
    return promise.getFuture();
  };

  auto lastState = _guardedData.getLockedGuard()->internalState;

  switch (lastState) {
    case FollowerInternalState::kUninitializedState: {
      noop().thenValue(
          transitionTo(FollowerInternalState::kWaitForLeaderConfirmation));
    } break;
    case FollowerInternalState::kWaitForLeaderConfirmation: {
      waitForLeaderAcked()
          .thenValue(transitionWith([this] {
            instantiateStateMachine();
            if (needsSnapshot()) {
              return FollowerInternalState::kTransferSnapshot;
            } else {
              startService();
              return FollowerInternalState::kWaitForNewEntries;
            }
          }))
          .thenFinal(handleErrors);
    } break;
    case FollowerInternalState::kTransferSnapshot: {
      tryTransferSnapshot()
          .then(transitionWith([this](futures::Try<futures::Unit>&& tryResult) {
            auto result =
                basics::catchVoidToResult([&] { std::move(tryResult).get(); });
            if (result.ok()) {
              startService();
              return FollowerInternalState::kWaitForNewEntries;
            } else {
              registerError(std::move(result));
              return FollowerInternalState::kSnapshotTransferFailed;
            }
          }))
          .thenFinal(handleErrors);
    } break;
    case FollowerInternalState::kSnapshotTransferFailed: {
      backOffSnapshotRetry()
          .thenValue(transitionTo(FollowerInternalState::kTransferSnapshot))
          .thenFinal(handleErrors);
    } break;
    case FollowerInternalState::kWaitForNewEntries: {
      waitForNewEntries()
          .thenValue(transitionWith([this](auto iter) {
            saveNewEntriesIter(std::move(iter));
            return FollowerInternalState::kApplyRecentEntries;
          }))
          .thenFinal(handleErrors);
    } break;
    case FollowerInternalState::kApplyRecentEntries: {
      applyNewEntries()
          .thenValue(throwResultOnError)
          .thenValue(transitionWith([this]() {
            resolveAppliedEntriesQueue();
            return FollowerInternalState::kWaitForNewEntries;
          }))
          .thenFinal(handleErrors);
    } break;
  }
}

template<typename S>
void FollowerStateManager<S>::saveNewEntriesIter(
    std::unique_ptr<typename Stream::Iterator> iter) {
  _guardedData.doUnderLock([&iter](auto& data) {
    data.nextEntriesIter = std::move(iter);
    data.ingestionRange.reset();
  });
}

template<typename S>
void FollowerStateManager<S>::resolveAppliedEntriesQueue() {
  auto resolvePromises = this->_guardedData.doUnderLock(
      [&](FollowerStateManager<S>::GuardedData& data) {
        TRI_ASSERT(data.ingestionRange);
        data._nextWaitForIndex = data.ingestionRange.value().to;
        auto resolveQueue =
            std::make_unique<FollowerStateManager::WaitForAppliedQueue>();
        LOG_CTX("9929a", TRACE, this->loggerContext)
            << "Resolving WaitForApplied promises upto "
            << data._nextWaitForIndex;
        auto const end =
            data.waitForAppliedQueue.lower_bound(data._nextWaitForIndex);
        for (auto it = data.waitForAppliedQueue.begin(); it != end;) {
          resolveQueue->insert(data.waitForAppliedQueue.extract(it++));
        }
        auto action_ =
            DeferredAction([resolveQueue = std::move(resolveQueue)]() noexcept {
              // TODO These should probably be scheduled.
              for (auto& p : *resolveQueue) {
                p.second.setValue();
              }
            });

        TRI_ASSERT(data.stream != nullptr);
        LOG_CTX("a1462", TRACE, this->loggerContext)
            << "polling for new entries _nextWaitForIndex = "
            << data._nextWaitForIndex;
        TRI_ASSERT(data.nextEntriesIter == nullptr);
        return action_;
      });

  resolvePromises.fire();
}

template<typename S>
void FollowerStateManager<S>::startService() {
  auto const state = _guardedData.doUnderLock([](auto& data) {
    data.lastError.reset();
    data.errorCounter = 0;
    return data.state;
  });
  // setStateManager must not be called while the lock is held,
  // or it will deadlock.
  state->setStateManager(this->shared_from_this());
}

template<typename S>
void FollowerStateManager<S>::registerError(Result error) {
  TRI_ASSERT(error.fail());
  _guardedData.doUnderLock([&error](auto& data) {
    data.lastError = std::move(error);
    data.errorCounter += 1;
  });
}

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

  auto const retryCount = this->_guardedData.getLockedGuard()->errorCounter;
  auto const duration = calcRetryDuration(retryCount);
  LOG_CTX("2ea59", TRACE, loggerContext)
      << "retry snapshot transfer after " << fmtTime(duration) << ", "
      << retryCount << countSuffix(retryCount) << " retry";
  return delayedFuture(duration);
}

template<typename S>
FollowerStateManager<S>::FollowerStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateBase> const& parent,
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory,
    std::shared_ptr<ReplicatedStateMetrics> ms) noexcept
    : _guardedData(*this, std::move(core), std::move(token)),
      parent(parent),
      logFollower(std::move(logFollower)),
      factory(std::move(factory)),
      loggerContext(std::move(loggerContext)),
      metrics(std::move(ms)) {
  TRI_ASSERT(metrics != nullptr);
  metrics->replicatedStateNumberFollowers->fetch_add(1);
}

template<typename S>
auto FollowerStateManager<S>::getStatus() const -> StateStatus {
  return _guardedData.doUnderLock([&](GuardedData const& data) {
    if (data._didResign) {
      TRI_ASSERT(data.core == nullptr && data.token == nullptr);
      throw replicated_log::ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
    } else {
      // Note that `this->core` is passed into the state when the follower is
      // started (more particularly, when the follower state is created), but
      // `this->state` is only set after replaying the log has finished.
      // Thus both can be null here.
      TRI_ASSERT(data.token != nullptr);
    }
    FollowerStatus status;
    status.managerState.state = data.internalState;
    status.managerState.lastChange = data.lastInternalStateChange;
    status.managerState.detail = std::nullopt;
    status.generation = data.token->generation;
    status.snapshot = data.token->snapshot;
    status.lastAppliedIndex = data._nextWaitForIndex.saturatedDecrement();

    if (data.lastError.has_value()) {
      status.managerState.detail = basics::StringUtils::concatT(
          "Last error was: ", data.lastError->errorMessage());
    }

    return StateStatus{.variant = std::move(status)};
  });
}

template<typename S>
auto FollowerStateManager<S>::getFollowerState() const
    -> std::shared_ptr<IReplicatedFollowerState<S>> {
  return _guardedData.doUnderLock(
      [](GuardedData const& data) -> decltype(data.state) {
        switch (data.internalState) {
          case FollowerInternalState::kWaitForNewEntries:
          case FollowerInternalState::kApplyRecentEntries:
            return data.state;
          case FollowerInternalState::kUninitializedState:
          case FollowerInternalState::kWaitForLeaderConfirmation:
          case FollowerInternalState::kTransferSnapshot:
          case FollowerInternalState::kSnapshotTransferFailed:
            return nullptr;
        }
        FATAL_ERROR_ABORT();
      });
}

template<typename S>
auto FollowerStateManager<S>::resign() && noexcept
    -> std::tuple<std::unique_ptr<CoreType>,
                  std::unique_ptr<ReplicatedStateToken>, DeferredAction> {
  auto resolveQueue = std::make_unique<WaitForAppliedQueue>();
  LOG_CTX("63622", TRACE, loggerContext) << "Follower manager resigning";
  auto guard = _guardedData.getLockedGuard();
  auto core = std::invoke([&] {
    if (guard->state != nullptr) {
      TRI_ASSERT(guard->core == nullptr);
      return std::move(*guard->state).resign();
    } else {
      return std::move(guard->core);
    }
  });
  TRI_ASSERT(core != nullptr);
  TRI_ASSERT(guard->token != nullptr);
  TRI_ASSERT(!guard->_didResign);
  guard->_didResign = true;
  std::swap(*resolveQueue, guard->waitForAppliedQueue);
  return std::make_tuple(
      std::move(core), std::move(guard->token),
      DeferredAction([resolveQueue = std::move(resolveQueue)]() noexcept {
        for (auto& p : *resolveQueue) {
          p.second.setException(replicated_log::ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              ADB_HERE));
        }
      }));
}

template<typename S>
auto FollowerStateManager<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> {
  return _guardedData.getLockedGuard()->stream;
}

template<typename S>
FollowerStateManager<S>::~FollowerStateManager() {
  metrics->replicatedStateNumberFollowers->fetch_sub(1);
}

template<typename S>
auto FollowerStateManager<S>::needsSnapshot() const noexcept -> bool {
  LOG_CTX("ea777", TRACE, loggerContext) << "check if new snapshot is required";
  auto const needsSnapshot =
      _guardedData.doUnderLock([&](GuardedData const& data) {
        LOG_CTX("aee5b", DEBUG, loggerContext)
            << "snapshot status is " << data.token->snapshot.status
            << ", generation is " << data.token->generation;
        return data.token->snapshot.status != SnapshotStatus::kCompleted;
      });

  if (needsSnapshot) {
    LOG_CTX("3d0fc", DEBUG, loggerContext) << "new snapshot is required";
  } else {
    LOG_CTX("9cd75", DEBUG, loggerContext) << "no snapshot transfer required";
  }

  return needsSnapshot;
}

template<typename S>
auto FollowerStateManager<S>::waitForLeaderAcked()
    -> futures::Future<futures::Unit> {
  GaugeScopedCounter waitForLeaderCounter(
      metrics->replicatedStateNumberWaitingForLeader);
  return logFollower->waitForLeaderAcked().thenValue(
      [waitForLeaderCounter = std::move(waitForLeaderCounter)](
          replicated_log::WaitForResult const&) mutable {
        waitForLeaderCounter.fire();
        return futures::Unit();
      });
}

template<typename S>
auto FollowerStateManager<S>::tryTransferSnapshot()
    -> futures::Future<futures::Unit> {
  auto const& leader = logFollower->getLeader();
  ADB_PROD_ASSERT(leader.has_value())
      << "Leader established it's leadership. "
         "There has to be a leader in the current term.";

  auto const commitIndex = logFollower->getCommitIndex();
  LOG_CTX("52a11", DEBUG, loggerContext)
      << "try to acquire a new snapshot, starting at " << commitIndex;
  return _guardedData.doUnderLock([&](auto& data) {
    ADB_PROD_ASSERT(data.state != nullptr)
        << "State machine must have been initialized before acquiring a "
           "snapshot.";

    MeasureTimeGuard rttGuard(metrics->replicatedStateAcquireSnapshotRtt);
    GaugeScopedCounter snapshotCounter(
        metrics->replicatedStateNumberWaitingForSnapshot);
    return data.state->acquireSnapshot(*leader, commitIndex)
        .then([ctx = loggerContext,
               errorCounter =
                   metrics->replicatedStateNumberAcquireSnapshotErrors,
               rttGuard = std::move(rttGuard),
               snapshotCounter = std::move(snapshotCounter)](
                  futures::Try<Result>&& tryResult) mutable {
          rttGuard.fire();
          snapshotCounter.fire();
          auto result = basics::catchToResult([&] { return tryResult.get(); });
          if (result.ok()) {
            LOG_CTX("44d58", DEBUG, ctx)
                << "snapshot transfer successfully completed";
          } else {
            LOG_CTX("9a68a", ERR, ctx)
                << "failed to transfer snapshot: " << result.errorMessage()
                << " - retry scheduled";
            errorCounter->operator++();
            throw basics::Exception(std::move(result), ADB_HERE);
          }
        });
  });
}

template<typename S>
void FollowerStateManager<S>::instantiateStateMachine() {
  _guardedData.doUnderLock([&](GuardedData& data) {
    auto demux = Demultiplexer::construct(logFollower);
    demux->listen();
    data.stream = demux->template getStreamById<1>();

    LOG_CTX("1d843", TRACE, loggerContext)
        << "creating follower state instance";
    data.state = factory->constructFollower(std::move(data.core));
  });
}

template<typename S>
auto FollowerStateManager<S>::waitForNewEntries()
    -> futures::Future<std::unique_ptr<typename Stream::Iterator>> {
  auto futureIter = _guardedData.doUnderLock([&](GuardedData& data) {
    TRI_ASSERT(data.stream != nullptr);
    LOG_CTX("a1462", TRACE, loggerContext)
        << "polling for new entries _nextWaitForIndex = "
        << data._nextWaitForIndex;
    TRI_ASSERT(data.nextEntriesIter == nullptr);
    return data.stream->waitForIterator(data._nextWaitForIndex);
  });

  return futureIter;
}

template<typename S>
auto FollowerStateManager<S>::applyNewEntries() -> futures::Future<Result> {
  auto [state, iter] = _guardedData.doUnderLock([&](GuardedData& data) {
    TRI_ASSERT(data.nextEntriesIter != nullptr);
    auto iter_ = std::move(data.nextEntriesIter);
    data.ingestionRange = iter_->range();
    return std::make_pair(data.state, std::move(iter_));
  });
  TRI_ASSERT(state != nullptr);
  auto range = iter->range();
  LOG_CTX("3678e", TRACE, loggerContext) << "apply entries in range " << range;
  MeasureTimeGuard rttGuard(metrics->replicatedStateApplyEntriesRtt);
  return state->applyEntries(std::move(iter))
      .then([ctx = loggerContext, rttGuard = std::move(rttGuard),
             metrics = metrics, range](auto&& tryResult) mutable {
        rttGuard.fire();
        auto result =
            basics::catchToResult([&] { return std::move(tryResult.get()); });
        if (result.fail()) {
          LOG_CTX("dd84e", ERR, ctx)
              << "failed to transfer snapshot: " << result.errorMessage()
              << " - retry scheduled";
          metrics->replicatedStateNumberApplyEntriesErrors->operator++();
        } else {
          metrics->replicatedStateNumberProcessedEntries->count(range.count());
        }
        return std::forward<decltype(result)>(result);
      });
}

namespace {
constexpr auto statesAreCoherent(FollowerInternalState const followerState,
                                 SnapshotStatus const snapshotState) -> bool {
  switch (followerState) {
    case FollowerInternalState::kUninitializedState:
    case FollowerInternalState::kWaitForLeaderConfirmation:
      // The token could be in any state at this point
      return true;
    case FollowerInternalState::kTransferSnapshot:
      return snapshotState == SnapshotStatus::kInProgress;
    case FollowerInternalState::kSnapshotTransferFailed:
      return snapshotState == SnapshotStatus::kFailed;
    case FollowerInternalState::kWaitForNewEntries:
    case FollowerInternalState::kApplyRecentEntries:
      return snapshotState == SnapshotStatus::kCompleted;
  }
  ADB_PROD_ASSERT(false) << "Unhandled state " << to_string(followerState);
  return false;
}
}  // namespace

template<typename S>
void FollowerStateManager<S>::GuardedData::updateInternalState(
    FollowerInternalState newState) {
  if (token == nullptr) {
    using namespace fmt::literals;
    // For some reason, gcc (11.2 at least) refuses to compile without
    // `fmt::runtime`.
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
        fmt::format(fmt::runtime("Replicated state already resigned, while "
                                 "switching from {old} to "
                                 "{new}. Was in {old} for {dur:%S}s."),
                    "old"_a = to_string(internalState),
                    "new"_a = to_string(newState),
                    "dur"_a = std::chrono::system_clock::now() -
                              lastInternalStateChange),
        ADB_HERE);
  }
  TRI_ASSERT(statesAreCoherent(internalState, token->snapshot.status));
  internalState = newState;
  lastInternalStateChange = std::chrono::system_clock::now();
  switch (newState) {
    case FollowerInternalState::kTransferSnapshot: {
      token->snapshot.updateStatus(SnapshotStatus::kInProgress);
    } break;
    case FollowerInternalState::kSnapshotTransferFailed: {
      token->snapshot.updateStatus(SnapshotStatus::kFailed);
    } break;
    case FollowerInternalState::kWaitForNewEntries: {
      token->snapshot.updateStatus(SnapshotStatus::kCompleted);
    } break;
    default:
      break;
  }
  TRI_ASSERT(statesAreCoherent(internalState, token->snapshot.status));
}

template<typename S>
FollowerStateManager<S>::GuardedData::GuardedData(
    FollowerStateManager& self, std::unique_ptr<CoreType> core,
    std::unique_ptr<ReplicatedStateToken> token)
    : self(self), core(std::move(core)), token(std::move(token)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

template<typename S>
void FollowerStateManager<S>::waitForLogFollowerResign() {
  logFollower->waitForResign().thenFinal(
      [weak = this->weak_from_this()](
          futures::Try<futures::Unit> const&) noexcept {
        if (auto self = weak.lock(); self != nullptr) {
          if (auto parentPtr = self->parent.lock(); parentPtr != nullptr) {
            LOG_CTX("654fb", TRACE, self->loggerContext)
                << "forcing rebuild because participant resigned";
            static_assert(noexcept(parentPtr->rebuildMe(self.get())));
            parentPtr->rebuildMe(self.get());
          }
        }
      });
}

}  // namespace arangodb::replication2::replicated_state
