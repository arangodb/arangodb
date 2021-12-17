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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"

#include "Replication2/Streams/LogMultiplexer.tpp"

namespace arangodb::replication2::replicated_state {

template<typename S>
struct ReplicatedState<S>::LeaderState
    : StateBase,
      std::enable_shared_from_this<LeaderState> {
  explicit LeaderState(std::shared_ptr<ReplicatedState> const& parent,
                       std::shared_ptr<replicated_log::ILogLeader> leader,
                       std::unique_ptr<ReplicatedStateCore> core) noexcept;

  using Stream = streams::ProducerStream<EntryType>;
  using Iterator = typename Stream::Iterator;

  auto getStatus() const -> StateStatus final;
  auto getSnapshotStatus() const -> SnapshotStatus final;


  void run();

  using Multiplexer = streams::LogMultiplexer<ReplicatedStateStreamSpec<S>>;
  std::shared_ptr<IReplicatedLeaderState<S>> state;
  std::shared_ptr<Stream> stream;
  std::weak_ptr<ReplicatedState> parent;
  std::shared_ptr<replicated_log::ILogLeader> logLeader;

  LeaderInternalState internalState{LeaderInternalState::kUninitializedState};
  std::chrono::system_clock::time_point lastInternalStateChange;
  std::optional<LogRange> recoveryRange;

  std::unique_ptr<ReplicatedStateCore> core;

 private:
  void updateInternalState(LeaderInternalState newState,
                           std::optional<LogRange> range = std::nullopt) {
    internalState = newState;
    lastInternalStateChange = std::chrono::system_clock::now();
    recoveryRange = range;
  }

  // TODO locking
};

template<typename S>
struct ReplicatedState<S>::FollowerState
    : StateBase,
      std::enable_shared_from_this<FollowerState> {
  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  FollowerState(std::shared_ptr<ReplicatedState> const& parent,
                std::shared_ptr<replicated_log::ILogFollower> logFollower,
                std::unique_ptr<ReplicatedStateCore> core) noexcept;

  void run();
  auto getStatus() const -> StateStatus final;
  auto getSnapshotStatus() const -> SnapshotStatus final;


  void awaitLeaderShip();
  void ingestLogData();
  void pollNewEntries();
  void checkSnapshot();
  void tryTransferSnapshot();
  void startService();


  void applyEntries(std::unique_ptr<Iterator> iter) noexcept;

  using Demultiplexer = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>;
  LogIndex nextEntry{0};

  // TODO locking

  std::shared_ptr<Stream> stream;
  std::shared_ptr<IReplicatedFollowerState<S>> state;
  std::weak_ptr<ReplicatedState> parent;
  std::shared_ptr<replicated_log::ILogFollower> logFollower;

  FollowerInternalState internalState{
      FollowerInternalState::kUninitializedState};
  std::chrono::system_clock::time_point lastInternalStateChange;
  std::optional<LogRange> ingestionRange;

  std::unique_ptr<ReplicatedStateCore> core;

 private:
  void updateInternalState(FollowerInternalState newState,
                           std::optional<LogRange> range = std::nullopt) {
    internalState = newState;
    lastInternalStateChange = std::chrono::system_clock::now();
    ingestionRange = range;
  }
};

template<typename S>
void ReplicatedState<S>::LeaderState::run() {
  // 1. wait for leadership established
  // 1.2. digest available entries into multiplexer
  // 2. construct leader state
  // 2.2 apply all log entries of the previous term
  // 3. make leader state available

  LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
      << "LeaderState waiting for leadership to be established";
  updateInternalState(LeaderInternalState::kWaitingForLeadershipEstablished);
  logLeader->waitForLeadership()
      .thenValue([self = this->shared_from_this()](auto&& result) {
        LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
            << "LeaderState established";
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
ReplicatedState<S>::LeaderState::LeaderState(
    std::shared_ptr<ReplicatedState> const& parent,
    std::shared_ptr<replicated_log::ILogLeader> leader,
    std::unique_ptr<ReplicatedStateCore> core) noexcept
    : parent(parent),
      logLeader(std::move(leader)),
      internalState(LeaderInternalState::kWaitingForLeadershipEstablished),
      core(std::move(core)) {}

template<typename S>
auto ReplicatedState<S>::LeaderState::getStatus() const -> StateStatus {
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
auto ReplicatedState<S>::LeaderState::getSnapshotStatus() const
    -> SnapshotStatus {
  return core->snapshot;
}

template<typename S>
void ReplicatedState<S>::FollowerState::applyEntries(
    std::unique_ptr<Iterator> iter) noexcept {
  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(iter != nullptr);
  auto range = iter->range();
  updateInternalState(FollowerInternalState::kApplyRecentEntries, range);
  state->applyEntries(std::move(iter))
      .thenFinal([self = this->shared_from_this(),
                  range](futures::Try<Result> tryResult) {
        try {
          auto& result = tryResult.get();
          if (result.ok()) {
            LOG_TOPIC("6e9bb", TRACE, Logger::REPLICATED_STATE)
                << "follower state applied range " << range;
            self->nextEntry = range.to;
            return self->pollNewEntries();
          } else {
            LOG_TOPIC("335f0", ERR, Logger::REPLICATED_STATE)
                << "follower failed to apply range " << range
                << " and returned error " << result;
          }
        } catch (std::exception const& e) {
          LOG_TOPIC("2fbae", ERR, Logger::REPLICATED_STATE)
              << "follower failed to apply range " << range
              << " with exception: " << e.what();
        } catch (...) {
          LOG_TOPIC("1a737", ERR, Logger::REPLICATED_STATE)
              << "follower failed to apply range " << range
              << " with unknown exception";
        }

        LOG_TOPIC("c89c8", DEBUG, Logger::REPLICATED_STATE)
            << "trigger retry for polling";
        // TODO retry
        std::abort();
      });
}

template<typename S>
void ReplicatedState<S>::FollowerState::pollNewEntries() {
  TRI_ASSERT(stream != nullptr);
  updateInternalState(FollowerInternalState::kNothingToApply);
  stream->waitForIterator(nextEntry).thenFinal(
      [self = this->shared_from_this()](
          futures::Try<std::unique_ptr<Iterator>> result) {
        try {
          self->applyEntries(std::move(result).get());
        } catch (basics::Exception const& e) {
          if (e.code() == TRI_ERROR_REPLICATION_LEADER_CHANGE) {
            if (auto ptr = self->parent.lock(); ptr) {
              ptr->flush(std::move(self->core));
            } else {
              LOG_TOPIC("15cb4", DEBUG, Logger::REPLICATED_STATE)
                  << "LogFollower resigned, but Replicated State already "
                     "gone";
            }
          } else {
            LOG_TOPIC("f2188", FATAL, Logger::REPLICATED_STATE)
                << "waiting for leader ack failed with unexpected exception: "
                << e.message();
          }
        }
      });
}

template<typename S>
void ReplicatedState<S>::FollowerState::tryTransferSnapshot() {
  auto& leader = logFollower->getLeader();
  TRI_ASSERT(leader.has_value()) << "leader established it's leadership. There "
                                    "has to be a leader in the current term";
  auto f = state->acquireSnapshot(*leader, logFollower->getCommitIndex());
  std::move(f).thenFinal(
      [self = this->shared_from_this()](futures::Try<Result>&& tryResult) {
        try {
          auto& result = tryResult.get();
          if (result.ok()) {
            LOG_TOPIC("44d58", INFO, Logger::REPLICATED_STATE)
                << "snapshot transfer successfully completed";
            self->core->snapshot.updateStatus(SnapshotStatus::kCompleted);
            return self->startService();
          }
        } catch (...) {
        }
        TRI_ASSERT(false) << "error handling not implemented";
        FATAL_ERROR_EXIT();
      });
}

template<typename S>
void ReplicatedState<S>::FollowerState::checkSnapshot() {
  LOG_TOPIC("aee5b", DEBUG, Logger::REPLICATED_STATE)
      << "snapshot status is " << core->snapshot << ", planned generation is "
      << core->plannedGeneration;
  bool needsSnapshot = core->snapshot.status != SnapshotStatus::kCompleted ||
                       core->snapshot.generation != core->plannedGeneration;
  if (needsSnapshot) {
    LOG_TOPIC("3d0fc", INFO, Logger::REPLICATED_STATE)
        << "new snapshot is required";
    tryTransferSnapshot();
  } else {
    LOG_TOPIC("9cd75", DEBUG, Logger::REPLICATED_STATE)
        << "no snapshot transfer required";
    startService();
  }
}

template<typename S>
void ReplicatedState<S>::FollowerState::startService() {
  LOG_TOPIC("26c55", DEBUG, Logger::REPLICATED_STATE)
      << "starting service as follower";
  state->_stream = stream;
  pollNewEntries();
}

template<typename S>
void ReplicatedState<S>::FollowerState::ingestLogData() {
  if (auto locked = parent.lock(); locked) {
    updateInternalState(FollowerInternalState::kTransferSnapshot);
    auto demux = Demultiplexer::construct(logFollower);
    demux->listen();
    stream = demux->template getStreamById<1>();

    LOG_TOPIC("1d843", TRACE, Logger::REPLICATED_STATE)
        << "creating follower state instance";
    state = locked->factory->constructFollower();

    LOG_TOPIC("ea777", TRACE, Logger::REPLICATED_STATE)
        << "check if new snapshot is required";
    checkSnapshot();
  } else {
    LOG_TOPIC("3237c", DEBUG, Logger::REPLICATED_STATE)
        << "Parent state already gone";
  }
}

template<typename S>
void ReplicatedState<S>::FollowerState::awaitLeaderShip() {
  updateInternalState(FollowerInternalState::kWaitForLeaderConfirmation);
  logFollower->waitForLeaderAcked().thenFinal(
      [self = this->shared_from_this()](
          futures::Try<replicated_log::WaitForResult>&& result) noexcept {
        try {
          try {
            result.throwIfFailed();
            LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
                << "leadership acknowledged - ingesting log data";
            self->ingestLogData();
          } catch (basics::Exception const& e) {
            if (e.code() == TRI_ERROR_REPLICATION_LEADER_CHANGE) {
              if (auto ptr = self->parent.lock(); ptr) {
                ptr->flush(std::move(self->core));
              } else {
                LOG_TOPIC("15cb4", DEBUG, Logger::REPLICATED_STATE)
                    << "LogFollower resigned, but Replicated State already "
                       "gone";
              }
            } else {
              LOG_TOPIC("f2188", FATAL, Logger::REPLICATED_STATE)
                  << "waiting for leader ack failed with unexpected exception: "
                  << e.message();
              FATAL_ERROR_EXIT();
            }
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("c7787", FATAL, Logger::REPLICATED_STATE)
              << "waiting for leader ack failed with unexpected exception: "
              << ex.what();
          FATAL_ERROR_EXIT();
        } catch (...) {
          LOG_TOPIC("43456", FATAL, Logger::REPLICATED_STATE)
              << "waiting for leader ack failed with unexpected exception";
          FATAL_ERROR_EXIT();
        }
      });
}

template<typename S>
void ReplicatedState<S>::FollowerState::run() {
  // 1. wait for log follower to have committed at least one entry
  // 2. receive a new snapshot (if required)
  //    if (old_generation != new_generation || snapshot_status != Completed)
  // 3. start polling for new entries
  awaitLeaderShip();
}

template<typename S>
ReplicatedState<S>::FollowerState::FollowerState(
    std::shared_ptr<ReplicatedState> const& parent,
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<ReplicatedStateCore> core) noexcept
    : parent(parent),
      logFollower(std::move(logFollower)),
      core(std::move(core)) {}

template<typename S>
auto ReplicatedState<S>::FollowerState::getSnapshotStatus() const
    -> SnapshotStatus {
  return core->snapshot;
}

template<typename S>
auto ReplicatedState<S>::FollowerState::getStatus() const -> StateStatus {
  FollowerStatus status;
  status.log = std::get<replicated_log::FollowerStatus>(
      logFollower->getStatus().getVariant());
  status.state.state = internalState;
  status.state.lastChange = lastInternalStateChange;
  status.state.detail = std::nullopt;
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
void ReplicatedState<S>::runFollower(
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<ReplicatedStateCore> core) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto machine = std::make_shared<FollowerState>(this->shared_from_this(),
                                                 logFollower, std::move(core));
  machine->run();
  currentState = machine;
}

template<typename S>
void ReplicatedState<S>::runLeader(
    std::shared_ptr<replicated_log::ILogLeader> logLeader,
    std::unique_ptr<ReplicatedStateCore> core) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE) << "create leader state";
  auto machine = std::make_shared<LeaderState>(this->shared_from_this(),
                                               logLeader, std::move(core));
  machine->run();
  currentState = machine;
}

template<typename S>
auto IReplicatedLeaderState<S>::getStream() const
    -> std::shared_ptr<Stream> const& {
  if (_stream) {
    return _stream;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
}

template<typename S>
auto IReplicatedFollowerState<S>::getStream() const
    -> std::shared_ptr<Stream> const& {
  if (_stream) {
    return _stream;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
}

template<typename S>
ReplicatedState<S>::ReplicatedState(
    std::shared_ptr<replicated_log::ReplicatedLog> log,
    std::shared_ptr<Factory> factory)
    : log(std::move(log)), factory(std::move(factory)) {}

template<typename S>
void ReplicatedState<S>::flush(std::unique_ptr<ReplicatedStateCore> core) {
  auto participant = log->getParticipant();
  if (auto leader =
          std::dynamic_pointer_cast<replicated_log::ILogLeader>(participant);
      leader) {
    runLeader(std::move(leader), std::move(core));
  } else if (auto follower =
                 std::dynamic_pointer_cast<replicated_log::ILogFollower>(
                     participant);
             follower) {
    runFollower(std::move(follower), std::move(core));
  } else {
    // unconfigured
    std::abort();
  }
}

template<typename S>
auto ReplicatedState<S>::getFollower() const -> std::shared_ptr<FollowerType> {
  if (auto machine = std::dynamic_pointer_cast<FollowerState>(currentState);
      machine) {
    return std::static_pointer_cast<FollowerType>(machine->state);
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getLeader() const -> std::shared_ptr<LeaderType> {
  if (auto internalState = std::dynamic_pointer_cast<LeaderState>(currentState);
      internalState) {
    if (internalState->state != nullptr) {
      return std::static_pointer_cast<LeaderType>(internalState->state);
    }
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getStatus() -> StateStatus {
  return currentState->getStatus();
}

template<typename S>
auto ReplicatedState<S>::getSnapshotStatus() const -> SnapshotStatus {
  return currentState->getSnapshotStatus();
}

}  // namespace arangodb::replication2::replicated_state
