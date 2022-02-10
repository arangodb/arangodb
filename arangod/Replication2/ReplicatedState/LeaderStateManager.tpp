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
void LeaderStateManager<S>::run() {
  // 1. wait for leadership established
  // 1.2. digest available entries into multiplexer
  // 2. construct leader state
  // 2.2 apply all log entries of the previous term
  // 3. make leader state available

  LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
      << "LeaderStateManager waiting for leadership to be established";
  updateInternalState(LeaderInternalState::kWaitingForLeadershipEstablished);
  logLeader->waitForLeadership()
      .thenValue([weak = this->weak_from_this()](auto&& result) {
        auto self = weak.lock();
        if (self == nullptr) {
          return futures::Future<Result>{
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
        }
        LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
            << "LeaderStateManager established";
        self->updateInternalState(LeaderInternalState::kIngestingExistingLog);
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
              self->updateInternalState(
                  LeaderInternalState::kRecoveryInProgress, result->range());
              std::shared_ptr<IReplicatedLeaderState<S>> machine =
                  self->factory->constructLeader();
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
                        self->updateInternalState(
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
    std::unique_ptr<ReplicatedStateCore> core,
    std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory) noexcept
    : parent(parent),
      logLeader(std::move(leader)),
      internalState(LeaderInternalState::kWaitingForLeadershipEstablished),
      core(std::move(core)),
      token(std::move(token)),
      factory(std::move(factory)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

template<typename S>
auto LeaderStateManager<S>::getStatus() const -> StateStatus {
  if (token == nullptr || core == nullptr) {
    TRI_ASSERT(core == nullptr && token == nullptr);
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
  }
  LeaderStatus status;
  status.managerState.state = internalState;
  status.managerState.lastChange = lastInternalStateChange;
  if (internalState == LeaderInternalState::kRecoveryInProgress &&
      recoveryRange) {
    status.managerState.detail =
        "recovery range is " + to_string(*recoveryRange);
  } else {
    status.managerState.detail = std::nullopt;
  }
  status.snapshot = token->snapshot;
  status.generation = token->generation;
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
auto LeaderStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<ReplicatedStateCore>,
                 std::unique_ptr<ReplicatedStateToken>> {
  LOG_TOPIC("edcf3", TRACE, Logger::REPLICATED_STATE)
      << "Leader manager resign";
  TRI_ASSERT(core != nullptr);
  TRI_ASSERT(token != nullptr);
  return {std::move(core), std::move(token)};
}

template<typename S>
void LeaderStateManager<S>::beginWaitingForParticipantResigned() {
  logLeader->waitFor(LogIndex{std::numeric_limits<std::uint64_t>::max()})
      .thenFinal([weak = this->weak_from_this()](auto&&) {
        if (auto self = weak.lock(); self != nullptr) {
          if (auto parentPtr = self->parent.lock(); parentPtr != nullptr) {
            parentPtr->forceRebuild();
          }
        }
      });
}
}  // namespace arangodb::replication2::replicated_state
