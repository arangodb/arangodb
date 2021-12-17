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
      .thenValue([self = this->shared_from_this()](auto&& result) {
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
            .thenValue([self](std::unique_ptr<Iterator>&& result) {
              if (auto parent = self->parent.lock(); parent) {
                LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                    << "creating leader instance and starting recovery";
                self->updateInternalState(
                    LeaderInternalState::kRecoveryInProgress, result->range());
                std::shared_ptr<IReplicatedLeaderState<S>> machine =
                    parent->factory->constructLeader();
                return machine->recoverEntries(std::move(result))
                    .then([self,
                           machine](futures::Try<Result>&& tryResult) mutable {
                      try {
                        if (auto result = tryResult.get(); result.ok()) {
                          LOG_TOPIC("1a375", DEBUG, Logger::REPLICATED_STATE)
                              << "recovery on leader completed";
                          self->state = machine;
                          self->core->snapshot.updateStatus(
                              SnapshotStatus::kCompleted);
                          self->updateInternalState(
                              LeaderInternalState::kServiceAvailable);
                          self->state->_stream = self->stream;
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
              }
              return futures::Future<Result>{
                  TRI_ERROR_REPLICATION_LEADER_CHANGE};
            });
      })
      .thenFinal(
          [self = this->shared_from_this()](futures::Try<Result>&& result) {
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
    std::unique_ptr<ReplicatedStateCore> core) noexcept
    : parent(parent),
      logLeader(std::move(leader)),
      internalState(LeaderInternalState::kWaitingForLeadershipEstablished),
      core(std::move(core)) {}

template<typename S>
auto LeaderStateManager<S>::getStatus() const -> StateStatus {
  LeaderStatus status;
  status.log = std::get<replicated_log::LeaderStatus>(
      logLeader->getStatus().getVariant());
  status.state.state = internalState;
  status.state.lastChange = lastInternalStateChange;
  if (internalState == LeaderInternalState::kRecoveryInProgress &&
      recoveryRange) {
    status.state.detail = "recovery range is " + to_string(*recoveryRange);
  } else {
    status.state.detail = std::nullopt;
  }
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
auto LeaderStateManager<S>::getSnapshotStatus() const
    -> SnapshotStatus {
  return core->snapshot;
}
}
