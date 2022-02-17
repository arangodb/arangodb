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
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

namespace arangodb::replication2::replicated_state {
template<typename S>
void LeaderStateManager<S>::run() noexcept {
  // 1. wait for leadership established
  // 1.2. digest available entries into multiplexer
  // 2. construct leader state
  // 2.2 apply all log entries of the previous term
  // 3. make leader state available

  LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
      << "LeaderStateManager waiting for leadership to be established";
  _guardedData.doUnderLock([](GuardedLeaderStateManagerData& self) {
    if (self._internalState == LeaderInternalState::kUninitializedState) {
      self.updateInternalState(
          LeaderInternalState::kWaitingForLeadershipEstablished);
    } else {
      LOG_TOPIC("e1861", FATAL, Logger::REPLICATED_STATE)
          << "LeaderStateManager was started twice, this must not happen. "
             "Bailing out.";
      FATAL_ERROR_EXIT();
    }
  });

  // TODO What happens when the LogLeader instance is resigned
  //      during run()? Are exceptions thrown that we must handle?

  logLeader->waitForLeadership()
      .thenValue([weak = this->weak_from_this()](auto&& result) {
        auto self = weak.lock();
        if (self == nullptr) {
          return futures::Future<Result>{
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
        }
        LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
            << "LeaderStateManager established";
        TRI_ASSERT(self->_guardedData.getLockedGuard()->_internalState ==
                   LeaderInternalState::kWaitingForLeadershipEstablished);
        self->_guardedData.getLockedGuard()->updateInternalState(
            LeaderInternalState::kIngestingExistingLog);
        auto mux = Multiplexer::construct(self->logLeader);
        mux->digestAvailableEntries();
        self->stream = mux->template getStreamById<1>();  // TODO fix stream id

        LOG_TOPIC("53ba2", TRACE, Logger::REPLICATED_STATE)
            << "receiving committed entries for recovery";
        // TODO we don't have to `waitFor` we can just access the log.
        //    new entries are not yet written, because the stream is
        //    not published.
        return self->stream->waitForIterator(LogIndex{0})
            .thenValue([weak](std::unique_ptr<Iterator>&& result) {
              LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                  << "creating leader instance and starting recovery";
              auto self = weak.lock();
              if (self == nullptr) {
                return futures::Future<Result>{
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
              }
              auto machine = self->_guardedData.doUnderLock(
                  [&](auto& data)
                      -> std::shared_ptr<IReplicatedLeaderState<S>> {
                    TRI_ASSERT(data->_internalState ==
                               LeaderInternalState::kIngestingExistingLog);
                    data->updateInternalState(
                        LeaderInternalState::kRecoveryInProgress,
                        result->range());
                    return self->factory->constructLeader(
                        std::move(data->_core));
                  });
              return machine->recoverEntries(std::move(result))
                  .then([weak,
                         machine](futures::Try<Result>&& tryResult) mutable
                        -> Result {
                    auto self = weak.lock();
                    if (self == nullptr) {
                      return Result{
                          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
                    }
                    try {
                      if (auto result = tryResult.get(); result.ok()) {
                        LOG_TOPIC("1a375", DEBUG, Logger::REPLICATED_STATE)
                            << "recovery on leader completed";
                        self->state = machine;
                        self->token->snapshot.updateStatus(
                            SnapshotStatus::kCompleted);
                        TRI_ASSERT(self->_guardedData.getLockedGuard()
                                       ->_internalState ==
                                   LeaderInternalState::kRecoveryInProgress);
                        self->_guardedData.getLockedGuard()
                            ->updateInternalState(
                                LeaderInternalState::kServiceAvailable);
                        self->state->_stream = self->stream;
                        self->beginWaitingForParticipantResigned();
                        return result;
                      } else {
                        LOG_TOPIC("3fd49", FATAL, Logger::REPLICATED_STATE)
                            << "recovery failed with error: "
                            << result.errorMessage();
                        FATAL_ERROR_EXIT();
                      }
                    } catch (std::exception const& e) {
                      LOG_TOPIC("3aaf8", FATAL, Logger::REPLICATED_STATE)
                          << "recovery failed with exception: " << e.what();
                      FATAL_ERROR_EXIT();
                    } catch (...) {
                      LOG_TOPIC("a207d", FATAL, Logger::REPLICATED_STATE)
                          << "recovery failed with unknown exception";
                      FATAL_ERROR_EXIT();
                    }
                  });
            });
      })
      .thenFinal(
          [weak = this->weak_from_this()](futures::Try<Result>&& result) {
            auto self = weak.lock();
            if (self == nullptr) {
              return;
            }
            try {
              // Note that if one of the previous actions returned
              // TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, this will
              // be because the shared_ptr is already gone. Thus weak.lock()
              // above will have failed, too; thus we'll never see that error
              // code here.
              auto res = result.get();  // throws exceptions
              TRI_ASSERT(res.ok());
            } catch (std::exception const& e) {
              LOG_TOPIC("e73bc", FATAL, Logger::REPLICATED_STATE)
                  << "Unexpected exception in leader startup procedure: "
                  << e.what();
              FATAL_ERROR_EXIT();
            } catch (...) {
              LOG_TOPIC("4d2b7", FATAL, Logger::REPLICATED_STATE)
                  << "Unexpected exception in leader startup procedure";
              FATAL_ERROR_EXIT();
            }
          });
}

template<typename S>
LeaderStateManager<S>::LeaderStateManager(
    std::shared_ptr<ReplicatedState<S>> const& parent,
    std::shared_ptr<replicated_log::ILogLeader> leader,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory) noexcept
    : parent(parent),
      logLeader(std::move(leader)),
      factory(std::move(factory)),
      _guardedData(std::move(core), std::move(token)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

template<typename S>
auto LeaderStateManager<S>::getStatus() const -> StateStatus {
  if (_didResign) {
    TRI_ASSERT(_guardedData.doUnderLock([](auto const& self) {
      return self._core == nullptr && self._token == nullptr;
    }));
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
  } else {
    // Note that `this->core` is passed into the state when the leader is
    // started (more particularly, when the leader state is created), but
    // `this->state` is only set after replaying the log has finished.
    // Thus both can be null here.
    TRI_ASSERT(_guardedData.doUnderLock(
        [](auto const& self) { return self._token != nullptr; }));
  }
  auto leaderStatus = _guardedData.getLockedGuard()->getLeaderStatus();
  return StateStatus{.variant = std::move(leaderStatus)};
}

template<typename S>
auto LeaderStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<ReplicatedStateToken>> {
  LOG_TOPIC("edcf3", TRACE, Logger::REPLICATED_STATE)
      << "Leader manager resign";

  auto [core, token] = _guardedData.doUnderLock([&](auto& self) {
    if (state != nullptr) {
      TRI_ASSERT(self._core == nullptr);
      return std::pair(std::move(*state).resign(), std::move(self._token));
    } else {
      return std::pair(std::move(self._core), std::move(self._token));
    }
  });

  TRI_ASSERT(core != nullptr);
  TRI_ASSERT(token != nullptr);
  TRI_ASSERT(!_didResign);
  _didResign = true;
  return {std::move(core), std::move(token)};
}

template<typename S>
void LeaderStateManager<S>::beginWaitingForParticipantResigned() {
  logLeader->waitForResign().thenFinal([weak = this->weak_from_this()](auto&&) {
    if (auto self = weak.lock(); self != nullptr) {
      if (auto parentPtr = self->parent.lock(); parentPtr != nullptr) {
        parentPtr->forceRebuild();
      }
    }
  });
}

template<typename S>
LeaderStateManager<S>::GuardedLeaderStateManagerData::
    GuardedLeaderStateManagerData(std::unique_ptr<CoreType> core,
                                  std::unique_ptr<ReplicatedStateToken> token)
    : _internalState(LeaderInternalState::kWaitingForLeadershipEstablished),
      _core(std::move(core)),
      _token(std::move(token)) {}

template<typename S>
void LeaderStateManager<S>::GuardedLeaderStateManagerData::updateInternalState(
    LeaderInternalState newState, std::optional<LogRange> range) {
  _internalState = newState;
  _lastInternalStateChange = std::chrono::system_clock::now();
  _recoveryRange = range;
}

template<typename S>
auto LeaderStateManager<S>::GuardedLeaderStateManagerData::getLeaderStatus()
    const -> LeaderStatus {
  LeaderStatus status;
  status.managerState.state = _internalState;
  status.managerState.lastChange = _lastInternalStateChange;
  if (_internalState == LeaderInternalState::kRecoveryInProgress &&
      _recoveryRange) {
    status.managerState.detail =
        "recovery range is " + to_string(*_recoveryRange);
  } else {
    status.managerState.detail = std::nullopt;
  }
  status.snapshot = _token->snapshot;
  status.generation = _token->generation;
  return status;
}

}  // namespace arangodb::replication2::replicated_state
