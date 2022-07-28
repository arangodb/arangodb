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

#include "LeaderStateManager.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Metrics/Gauge.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Replication2/MetricsHelper.h"

namespace arangodb::replication2::replicated_state {
template<typename S>
void LeaderStateManager<S>::run() noexcept {
  // Very first thing: Make sure the state is rebuilt on log-resign.
  beginWaitingForLogLeaderResigned();

  static auto constexpr throwResultOnErrorAsCorrespondingException =
      [](Result result) {
        if (result.fail()) {
          TRI_ASSERT(!result.is(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED));
          if (result.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED)) {
            throw replicated_log::ParticipantResignedException(
                std::move(result), ADB_HERE);
          } else {
            throw basics::Exception(std::move(result), ADB_HERE);
          }
        }
      };

  auto const handleErrors = [weak = this->weak_from_this()](
                                futures::Try<futures::Unit>&&
                                    tryResult) noexcept {
    // TODO Instead of capturing "this", should we capture the loggerContext
    // instead?
    if (auto self = weak.lock(); self != nullptr) {
      try {
        std::ignore = tryResult.get();
      } catch (replicated_log::ParticipantResignedException const&) {
        LOG_CTX("7322d", DEBUG, self->loggerContext)
            << "Log leader resigned, stopping replicated state machine. Will "
               "be restarted soon.";
        return;
      } catch (basics::Exception const& ex) {
        LOG_CTX("0dcf7", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine: "
            << ex.message() << " (exception location: " << ex.location() << ")";
        FATAL_ERROR_EXIT();
      } catch (std::exception const& ex) {
        LOG_CTX("506c2", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine: "
            << ex.what();
        FATAL_ERROR_EXIT();
      } catch (...) {
        LOG_CTX("6788b", FATAL, self->loggerContext)
            << "Caught unhandled exception in replicated state machine.";
        FATAL_ERROR_EXIT();
      }
    }
  };

  auto const transitionTo = [this](LeaderInternalState state) noexcept {
    return [state, weak = this->weak_from_this()](futures::Unit) noexcept {
      if (auto self = weak.lock(); self != nullptr) {
        self->guardedData.getLockedGuard()->updateInternalState(state);
        self->run();
      }
    };
  };

  auto const toTryUnit = []<typename F>(F&& fn) -> futures::Try<futures::Unit> {
    try {
      return futures::Try<futures::Unit>{std::forward<F>(fn)()};
    } catch (...) {
      return futures::Try<futures::Unit>{std::current_exception()};
    }
  };

  auto const state = guardedData.getLockedGuard()->internalState;
  switch (state) {
    case LeaderInternalState::kUninitializedState: {
      // This transition does nothing except make it visible that run() was
      // actually called.
      std::invoke(
          transitionTo(LeaderInternalState::kWaitingForLeadershipEstablished),
          futures::Unit());
    } break;
    case LeaderInternalState::kWaitingForLeadershipEstablished: {
      waitForLeadership()
          .thenValue(transitionTo(LeaderInternalState::kRecoveryInProgress))
          .thenFinal(handleErrors);
    } break;
    case LeaderInternalState::kRecoveryInProgress: {
      recoverEntries()
          .thenValue(throwResultOnErrorAsCorrespondingException)
          .thenValue(transitionTo(LeaderInternalState::kServiceStarting))
          .thenFinal(handleErrors);
    } break;
    case LeaderInternalState::kServiceStarting: {
      auto tryResult = toTryUnit([this]() -> futures::Unit {
        auto res = startService();
        throwResultOnErrorAsCorrespondingException(std::move(res));
        return {};
      });
      if (tryResult.hasValue()) {
        std::invoke(transitionTo(LeaderInternalState::kServiceAvailable),
                    futures::Unit());
      } else {
        handleErrors(std::move(tryResult));
      }
    } break;
    case LeaderInternalState::kServiceAvailable: {
    } break;
  }
}

template<typename S>
auto LeaderStateManager<S>::waitForLeadership() noexcept
    -> futures::Future<futures::Unit> try {
  GaugeScopedCounter counter(metrics->replicatedStateNumberWaitingForLeader);
  return logLeader->waitForLeadership().thenValue(
      [counter =
           std::move(counter)](replicated_log::WaitForResult const&) mutable {
        counter.fire();
        return futures::Unit();
      });
} catch (...) {
  return futures::Try<futures::Unit>(std::current_exception());
}

template<typename S>
auto LeaderStateManager<S>::recoverEntries() noexcept
    -> futures::Future<Result> try {
  LOG_CTX("53ba1", TRACE, loggerContext) << "LeaderStateManager established";
  auto f = guardedData.doUnderLock([&](GuardedData& data) {
    TRI_ASSERT(data.internalState == LeaderInternalState::kRecoveryInProgress)
        << "Unexpected state " << to_string(data.internalState);
    auto mux = Multiplexer::construct(logLeader);
    mux->digestAvailableEntries();
#ifdef _MSC_VER                             // circumventing bug in msvc
    data.stream = mux->getStreamById<1>();  // TODO fix stream id
#else
    data.stream = mux->template getStreamById<1>();  // TODO fix stream id
#endif
    return data.stream->waitForIterator(LogIndex{0});
  });

  if (!f.isReady()) {
    LOG_CTX("4448d", ERR, loggerContext)
        << "Logic error: Stream is not ready yet. Will wait for it. "
           "Please report this error to arangodb.com";
    TRI_ASSERT(false);
  }
  auto&& result = f.get();

  LOG_CTX("53ba2", TRACE, loggerContext)
      << "receiving committed entries for recovery";

  LOG_CTX("53ba0", TRACE, loggerContext) << "creating leader instance";
  auto machine = guardedData.doUnderLock([&](GuardedData& data) {
    data.recoveryRange = result->range();

    auto core = std::move(data.core);

    if (core == nullptr) {
      LOG_CTX("6d9ee", DEBUG, loggerContext) << "core already gone ";
      throw replicated_log::ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
    }
    std::shared_ptr<IReplicatedLeaderState<S>> machine =
        factory->constructLeader(std::move(core));

    data.state = machine;

    return machine;
  });

  LOG_CTX("5af0d", DEBUG, loggerContext)
      << "starting recovery on range " << result->range();

  MeasureTimeGuard timeGuard(metrics->replicatedStateRecoverEntriesRtt);
  return machine->recoverEntries(std::move(result))
      .then([guard = std::move(timeGuard)](auto&& res) mutable {
        guard.fire();
        return std::move(res.get());
      });
} catch (...) {
  return futures::Try<Result>(std::current_exception());
}

template<typename S>
auto LeaderStateManager<S>::startService() -> Result {
  LOG_CTX("1a375", DEBUG, loggerContext) << "recovery on leader completed";
  auto state = guardedData.doUnderLock(
      [&](GuardedData& data) -> std::shared_ptr<IReplicatedLeaderState<S>> {
        if (data.token == nullptr) {
          LOG_CTX("59a31", DEBUG, loggerContext) << "token already gone";
          return nullptr;
        }
        data.token->snapshot.updateStatus(SnapshotStatus::kCompleted);
        data.recoveryRange = std::nullopt;
        TRI_ASSERT(data.state != nullptr);
        data.state->setStream(data.stream);
        return data.state;
      });

  if (state != nullptr) {
    state->onSnapshotCompleted();
    return Result{};
  } else {
    return Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
  }
}

template<typename S>
LeaderStateManager<S>::LeaderStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedState<S>> const& parent,
    std::shared_ptr<replicated_log::ILogLeader> leader,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory,
    std::shared_ptr<ReplicatedStateMetrics> ms) noexcept
    : guardedData(*this, LeaderInternalState::kWaitingForLeadershipEstablished,
                  std::move(core), std::move(token)),
      parent(parent),
      logLeader(std::move(leader)),
      loggerContext(std::move(loggerContext)),
      factory(std::move(factory)),
      metrics(std::move(ms)) {
  metrics->replicatedStateNumberLeaders->fetch_add(1);
}

template<typename S>
auto LeaderStateManager<S>::getStatus() const -> StateStatus {
  auto guard = guardedData.getLockedGuard();
  if (guard->_didResign) {
    TRI_ASSERT(guard->core == nullptr && guard->token == nullptr);
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
  } else {
    // Note that `this->core` is passed into the state when the leader is
    // started (more particularly, when the leader state is created), but
    // `this->state` is only set after replaying the log has finished.
    // Thus both can be null here.
    TRI_ASSERT(guard->token != nullptr);
  }
  LeaderStatus status;
  status.managerState.state = guard->internalState;
  status.managerState.lastChange = guard->lastInternalStateChange;
  if (guard->internalState == LeaderInternalState::kRecoveryInProgress &&
      guard->recoveryRange) {
    status.managerState.detail =
        "recovery range is " + to_string(*guard->recoveryRange);
  } else {
    status.managerState.detail = std::nullopt;
  }
  status.snapshot = guard->token->snapshot;
  status.generation = guard->token->generation;
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
auto LeaderStateManager<S>::resign() && noexcept
    -> std::tuple<std::unique_ptr<CoreType>,
                  std::unique_ptr<ReplicatedStateToken>, DeferredAction> {
  LOG_CTX("edcf3", TRACE, loggerContext) << "Leader manager resign";
  auto guard = guardedData.getLockedGuard();
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
  return std::make_tuple(std::move(core), std::move(guard->token),
                         DeferredAction{});
}

template<typename S>
void LeaderStateManager<S>::beginWaitingForLogLeaderResigned() {
  logLeader->waitForResign().thenFinal(
      [weak = this->weak_from_this()](auto&&) noexcept {
        if (auto self = weak.lock(); self != nullptr) {
          if (auto parentPtr = self->parent.lock(); parentPtr != nullptr) {
            static_assert(noexcept(parentPtr->rebuildMe(self.get())));
            parentPtr->rebuildMe(self.get());
          }
        }
      });
}

template<typename S>
auto LeaderStateManager<S>::getImplementationState()
    -> std::shared_ptr<IReplicatedLeaderState<S>> {
  return guardedData.doUnderLock([](auto& self) -> decltype(self.state) {
    // The state machine must not be accessible before it is ready.
    if (self.internalState == LeaderInternalState::kServiceAvailable) {
      return self.state;
    } else {
      return nullptr;
    }
  });
}

template<typename S>
LeaderStateManager<S>::~LeaderStateManager() {
  metrics->replicatedStateNumberLeaders->fetch_sub(1);
}

template<typename S>
LeaderStateManager<S>::GuardedData::GuardedData(
    LeaderStateManager& self, LeaderInternalState internalState,
    std::unique_ptr<CoreType> core,
    std::unique_ptr<ReplicatedStateToken> token) noexcept
    : self(self),
      internalState(internalState),
      core(std::move(core)),
      token(std::move(token)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

template<typename S>
void LeaderStateManager<S>::GuardedData::updateInternalState(
    LeaderInternalState newState) noexcept {
  internalState = newState;
  lastInternalStateChange = std::chrono::system_clock::now();
}

}  // namespace arangodb::replication2::replicated_state
