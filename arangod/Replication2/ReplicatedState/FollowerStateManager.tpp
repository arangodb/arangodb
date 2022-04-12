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

#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Basics/Exceptions.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
void FollowerStateManager<S>::applyEntries(
    std::unique_ptr<Iterator> iter) noexcept {
  TRI_ASSERT(iter != nullptr);
  auto range = iter->range();
  LOG_CTX("3678e", TRACE, loggerContext) << "apply entries in range " << range;

  auto state = _guardedData.doUnderLock([&](GuardedData& data) {
    data.updateInternalState(FollowerInternalState::kApplyRecentEntries, range);
    return data.state;
  });

  TRI_ASSERT(state != nullptr);

  state->applyEntries(std::move(iter))
      .thenFinal([weak = this->weak_from_this(),
                  range](futures::Try<Result> tryResult) noexcept {
        auto self = weak.lock();
        if (self == nullptr) {
          LOG_CTX("a87aa", TRACE, self->loggerContext)
              << "replicated state already gone";
          return;
        }
        try {
          auto& result = tryResult.get();
          if (result.ok()) {
            LOG_CTX("6e9bb", TRACE, self->loggerContext)
                << "follower state applied range " << range;

            auto [outerAction, pollFuture] =
                self->pollNewEntries([&](GuardedData& data) {
                  return data.updateNextIndex(range.to);
                });
            // this action will resolve promises that wait for a given index to
            // be applied
            outerAction.fire();
            self->handlePollResult(std::move(pollFuture));
            return;
          } else {
            LOG_CTX("335f0", ERR, self->loggerContext)
                << "follower failed to apply range " << range
                << " and returned error " << result;
          }
        } catch (std::exception const& e) {
          LOG_CTX("2fbae", ERR, self->loggerContext)
              << "follower failed to apply range " << range
              << " with exception: " << e.what();
        } catch (...) {
          LOG_CTX("1a737", ERR, self->loggerContext)
              << "follower failed to apply range " << range
              << " with unknown exception";
        }

        LOG_CTX("c89c8", DEBUG, self->loggerContext)
            << "trigger retry for polling";
        // TODO retry
        std::abort();
      });
}

template<typename S>
template<typename F>
auto FollowerStateManager<S>::pollNewEntries(F&& fn) {
  return _guardedData.doUnderLock([&](GuardedData& data) {
    auto result = std::invoke(std::forward<F>(fn), data);
    TRI_ASSERT(data.stream != nullptr);
    LOG_CTX("a1462", TRACE, loggerContext)
        << "polling for new entries _nextWaitForIndex = "
        << data._nextWaitForIndex;
    data.updateInternalState(FollowerInternalState::kNothingToApply);
    return std::make_tuple(std::move(result), data.stream->waitForIterator(
                                                  data._nextWaitForIndex));
  });
}

template<typename S>
void FollowerStateManager<S>::handlePollResult(
    futures::Future<std::unique_ptr<Iterator>>&& pollFuture) {
  std::move(pollFuture)
      .thenFinal([weak = this->weak_from_this()](
                     futures::Try<std::unique_ptr<Iterator>> result) noexcept {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }
        try {
          self->applyEntries(std::move(result).get());
        } catch (replicated_log::ParticipantResignedException const&) {
          if (auto ptr = self->parent.lock(); ptr) {
            LOG_CTX("654fb", TRACE, self->loggerContext)
                << "forcing rebuild because participant resigned";
            ptr->forceRebuild();
          } else {
            LOG_CTX("15cb4", TRACE, self->loggerContext)
                << "LogFollower resigned, but Replicated State already gone";
          }
        } catch (basics::Exception const& e) {
          LOG_CTX("f2188", FATAL, self->loggerContext)
              << "waiting for leader ack failed with unexpected exception: "
              << e.message();
        }
      });
}

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

template<typename S>
void FollowerStateManager<S>::tryTransferSnapshot(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  auto& leader = logFollower->getLeader();
  TRI_ASSERT(leader.has_value()) << "leader established it's leadership. There "
                                    "has to be a leader in the current term";

  LOG_CTX("52a11", DEBUG, loggerContext)
      << "try to acquire a new snapshot, starting at "
      << logFollower->getCommitIndex();
  auto f = hiddenState->acquireSnapshot(*leader, logFollower->getCommitIndex());
  std::move(f).thenFinal([weak = this->weak_from_this(), hiddenState](
                             futures::Try<Result>&& tryResult) noexcept {
    auto self = weak.lock();
    if (self == nullptr) {
      return;
    }

    auto result = basics::catchToResult([&] { return tryResult.get(); });
    if (result.ok()) {
      LOG_CTX("44d58", DEBUG, self->loggerContext)
          << "snapshot transfer successfully completed";

      bool startService =
          self->_guardedData.doUnderLock([&](GuardedData& data) {
            if (data.token == nullptr) {
              return false;
            }
            data.token->snapshot.updateStatus(SnapshotStatus::kCompleted);
            return true;
          });
      if (startService) {
        self->startService(hiddenState);
      }
      return;
    } else {
      LOG_CTX("9a68a", ERR, self->loggerContext)
          << "failed to transfer snapshot: " << result.errorMessage()
          << " - retry scheduled";

      auto retryCount = self->_guardedData.doUnderLock([&](GuardedData& data) {
        data.updateInternalState(FollowerInternalState::kSnapshotTransferFailed,
                                 result);
        return data.errorCounter;
      });

      self->retryTransferSnapshot(std::move(hiddenState), retryCount);
    }
  });
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
void FollowerStateManager<S>::retryTransferSnapshot(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState,
    std::uint64_t retryCount) {
  auto duration = calcRetryDuration(retryCount);
  LOG_CTX("2ea59", TRACE, loggerContext)
      << "retry snapshot transfer after "
      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
      << "ms";
  delayedFuture(duration).thenFinal(
      [weak = this->weak_from_this(), hiddenState](auto&&) {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }

        self->tryTransferSnapshot(hiddenState);
      });
}

template<typename S>
void FollowerStateManager<S>::checkSnapshot(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  bool needsSnapshot = _guardedData.doUnderLock([&](GuardedData& data) {
    LOG_CTX("aee5b", DEBUG, loggerContext)
        << "snapshot status is " << data.token->snapshot.status
        << ", generation is " << data.token->generation;
    return data.token->snapshot.status != SnapshotStatus::kCompleted;
  });
  if (needsSnapshot) {
    LOG_CTX("3d0fc", DEBUG, loggerContext) << "new snapshot is required";
    tryTransferSnapshot(hiddenState);
  } else {
    LOG_CTX("9cd75", DEBUG, loggerContext) << "no snapshot transfer required";
    startService(hiddenState);
  }
}

template<typename S>
void FollowerStateManager<S>::startService(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  hiddenState->setStateManager(this->shared_from_this());

  auto [nothing, pollFuture] = pollNewEntries([&](GuardedData& data) {
    LOG_CTX("26c55", TRACE, loggerContext) << "starting service as follower";
    data.state = hiddenState;
    return std::monostate{};
  });

  handlePollResult(std::move(pollFuture));
}

template<typename S>
void FollowerStateManager<S>::ingestLogData() {
  auto core = _guardedData.doUnderLock([&](GuardedData& data) {
    data.updateInternalState(FollowerInternalState::kTransferSnapshot);
    auto demux = Demultiplexer::construct(logFollower);
    demux->listen();
    data.stream = demux->template getStreamById<1>();
    return std::move(data.core);
  });

  LOG_CTX("1d843", TRACE, loggerContext) << "creating follower state instance";
  auto hiddenState = factory->constructFollower(std::move(core));

  LOG_CTX("ea777", TRACE, loggerContext) << "check if new snapshot is required";
  checkSnapshot(hiddenState);
}

template<typename S>
void FollowerStateManager<S>::awaitLeaderShip() {
  _guardedData.getLockedGuard()->updateInternalState(
      FollowerInternalState::kWaitForLeaderConfirmation);
  try {
    handleAwaitLeadershipResult(logFollower->waitForLeaderAcked());
  } catch (replicated_log::ParticipantResignedException const&) {
    if (auto p = parent.lock(); p) {
      LOG_CTX("1cb5c", TRACE, loggerContext)
          << "forcing rebuild because participant resigned";
      return p->forceRebuild();
    } else {
      LOG_CTX("a62cb", TRACE, loggerContext) << "replicated state already gone";
    }
  }
}

template<typename S>
void FollowerStateManager<S>::handleAwaitLeadershipResult(
    futures::Future<replicated_log::WaitForResult>&& f) {
  std::move(f).thenFinal(
      [weak = this->weak_from_this()](
          futures::Try<replicated_log::WaitForResult>&& result) noexcept {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }
        try {
          try {
            result.throwIfFailed();
            LOG_CTX("53ba1", TRACE, self->loggerContext)
                << "leadership acknowledged - ingesting log data";
            self->ingestLogData();
          } catch (replicated_log::ParticipantResignedException const&) {
            if (auto ptr = self->parent.lock(); ptr) {
              LOG_CTX("79e37", DEBUG, self->loggerContext)
                  << "participant resigned before leadership - force rebuild";
              ptr->forceRebuild();
            } else {
              LOG_CTX("15cb4", DEBUG, self->loggerContext)
                  << "LogFollower resigned, but Replicated State already "
                     "gone";
            }
          } catch (basics::Exception const& e) {
            LOG_CTX("f2188", FATAL, self->loggerContext)
                << "waiting for leader ack failed with unexpected exception: "
                << e.message();
            FATAL_ERROR_EXIT();
          }
        } catch (std::exception const& ex) {
          LOG_CTX("c7787", FATAL, self->loggerContext)
              << "waiting for leader ack failed with unexpected exception: "
              << ex.what();
          FATAL_ERROR_EXIT();
        } catch (...) {
          LOG_CTX("43456", FATAL, self->loggerContext)
              << "waiting for leader ack failed with unexpected exception";
          FATAL_ERROR_EXIT();
        }
      });
}

template<typename S>
void FollowerStateManager<S>::run() {
  // 1. wait for log follower to have committed at least one entry
  // 2. receive a new snapshot (if required)
  //    if (old_generation != new_generation || snapshot_status != Completed)
  // 3. start polling for new entries
  awaitLeaderShip();
}

template<typename S>
FollowerStateManager<S>::FollowerStateManager(
    LoggerContext loggerContext, std::shared_ptr<ReplicatedStateBase> parent,
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory) noexcept
    : _guardedData(*this, std::move(core), std::move(token)),
      parent(parent),
      logFollower(std::move(logFollower)),
      factory(std::move(factory)),
      loggerContext(std::move(loggerContext)) {}

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
  return _guardedData.getLockedGuard()->state;
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
void FollowerStateManager<S>::GuardedData::updateInternalState(
    FollowerInternalState newState, std::optional<LogRange> range) {
  internalState = newState;
  lastInternalStateChange = std::chrono::system_clock::now();
  ingestionRange = range;
  lastError.reset();
  errorCounter = 0;
}

template<typename S>
void FollowerStateManager<S>::GuardedData::updateInternalState(
    FollowerInternalState newState, Result error) {
  internalState = newState;
  lastInternalStateChange = std::chrono::system_clock::now();
  ingestionRange.reset();
  lastError.emplace(std::move(error));
  errorCounter += 1;
}

template<typename S>
auto FollowerStateManager<S>::GuardedData::updateNextIndex(
    LogIndex nextWaitForIndex) -> DeferredAction {
  _nextWaitForIndex = nextWaitForIndex;
  auto resolveQueue = std::make_unique<WaitForAppliedQueue>();
  LOG_CTX("9929a", TRACE, self.loggerContext)
      << "Resolving WaitForApplied promises upto " << nextWaitForIndex;
  auto const end = waitForAppliedQueue.lower_bound(nextWaitForIndex);
  for (auto it = waitForAppliedQueue.begin(); it != end;) {
    resolveQueue->insert(waitForAppliedQueue.extract(it++));
  }
  return DeferredAction([resolveQueue = std::move(resolveQueue)]() noexcept {
    for (auto& p : *resolveQueue) {
      p.second.setValue();
    }
  });
}

template<typename S>
FollowerStateManager<S>::GuardedData::GuardedData(
    FollowerStateManager& self, std::unique_ptr<CoreType> core,
    std::unique_ptr<ReplicatedStateToken> token)
    : self(self), core(std::move(core)), token(std::move(token)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

}  // namespace arangodb::replication2::replicated_state
