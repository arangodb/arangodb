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

template <typename S, typename F>
struct ReplicatedState<S, F>::LeaderState : StateBase,
                                            std::enable_shared_from_this<LeaderState> {
  explicit LeaderState(std::shared_ptr<ReplicatedState> const& parent,
                       std::shared_ptr<replicated_log::LogLeader> leader)
      : _parent(parent), _leader(std::move(leader)) {}

  using Stream = streams::ProducerStream<EntryType>;
  using Iterator = typename Stream::Iterator;

  void run() {
    // 1. wait for leadership established
    // 1.2. digest available entries into multiplexer
    // 2. construct leader state
    // 2.2 apply all log entries of the previous term
    // 3. make leader state available

    LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
        << "LeaderState waiting for leadership to be established";
    _leader->waitForLeadership()
        .thenValue([self = this->shared_from_this()](auto&& result) {
          LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
              << "LeaderState established";
          auto mux = Multiplexer::construct(self->_leader);
          mux->digestAvailableEntries();
          self->stream = mux->template getStreamById<1>();  // TODO fix stream id

          LOG_TOPIC("53ba2", TRACE, Logger::REPLICATED_STATE)
              << "receiving committed entries for recovery";
          // TODO we don't have to `waitFor` we can just access the log.
          //    new entries are not yet written, because the stream is
          //    not published.
          return self->stream->waitForIterator(LogIndex{0}).thenValue([self](std::unique_ptr<Iterator>&& result) {
            if (auto parent = self->_parent.lock(); parent) {
              LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                  << "creating leader instance and starting recovery";
              std::shared_ptr<ReplicatedLeaderState<S>> machine =
                  parent->factory->constructLeader();
              return machine->recoverEntries(std::move(result))
                  .then([self, machine](futures::Try<Result>&& tryResult) mutable {
                    try {
                      if (auto result = tryResult.get(); result.ok()) {
                        LOG_TOPIC("1a375", DEBUG, Logger::REPLICATED_STATE)
                            << "recovery on leader completed";
                        self->_machine = machine;
                        self->_machine->_stream = self->stream;
                        return result;
                      } else {
                        LOG_TOPIC("3fd49", FATAL, Logger::REPLICATED_STATE)
                            << "recovery failed with error: " << result.errorMessage();
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
            return futures::Future<Result>{TRI_ERROR_REPLICATION_LEADER_CHANGE};
          });
        })
        .thenFinal([self = this->shared_from_this()](futures::Try<Result>&& result) {
          try {
            auto res = result.get();  // throws exceptions
            TRI_ASSERT(res.ok());
          } catch (std::exception const& e) {
            LOG_TOPIC("e73bc", FATAL, Logger::REPLICATED_STATE)
                << "Unexpected exception in leader startup procedure: " << e.what();
            FATAL_ERROR_EXIT();
          } catch (...) {
            LOG_TOPIC("4d2b7", FATAL, Logger::REPLICATED_STATE)
                << "Unexpected exception in leader startup procedure";
            FATAL_ERROR_EXIT();
          }
        });
  }

  using Multiplexer = streams::LogMultiplexer<ReplicatedStateStreamSpec<S>>;

  std::shared_ptr<ReplicatedLeaderState<S>> _machine;
  std::shared_ptr<Stream> stream;
  std::weak_ptr<ReplicatedState> _parent;
  std::shared_ptr<replicated_log::LogLeader> _leader;
};

template <typename S, typename F>
struct ReplicatedState<S, F>::FollowerState
    : StateBase,
      std::enable_shared_from_this<FollowerState> {
  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  using Demultiplexer = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>;

  FollowerState(std::shared_ptr<ReplicatedState> const& parent,
                std::shared_ptr<replicated_log::LogFollower> logFollower)
      : parent(parent), logFollower(std::move(logFollower)) {}

  LogIndex nextEntry{0};

  std::shared_ptr<Stream> stream;
  std::shared_ptr<ReplicatedFollowerState<S>> state;
  std::weak_ptr<ReplicatedState> parent;
  std::shared_ptr<replicated_log::LogFollower> logFollower;

  void run() {
    // 1. wait for log follower to have committed at least one entry
    // 2. receive a new snapshot (if required)
    // 3. start polling for new entries
    awaitLeaderShip();
  }

  void awaitLeaderShip() {
    logFollower->waitForLeaderAcked().thenFinal([self = this->shared_from_this()](
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
              ptr->flush();
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

  void ingestLogData() {
    if (auto locked = parent.lock(); locked) {
      auto demux = Demultiplexer::construct(logFollower);
      demux->listen();
      stream = demux->template getStreamById<1>();

      LOG_TOPIC("1d843", TRACE, Logger::REPLICATED_STATE)
          << "creating follower state instance";
      state = locked->factory->constructFollower();

      LOG_TOPIC("ea777", TRACE, Logger::REPLICATED_STATE)
          << "check if new snapshot is required";
      // TODO implement check for snapshot transfer

      LOG_TOPIC("26c55", DEBUG, Logger::REPLICATED_STATE)
          << "starting service as follower";
      state->_stream = stream;
      pollNewEntries();
    } else {
      LOG_TOPIC("3237c", DEBUG, Logger::REPLICATED_STATE)
          << "Parent state already gone";
    }
  }

  void pollNewEntries() {
    TRI_ASSERT(stream != nullptr);
    stream->waitForIterator(nextEntry).thenFinal(
        [self = this->shared_from_this()](futures::Try<std::unique_ptr<Iterator>> result) {
          try {
            self->applyEntries(std::move(result).get());
          } catch (basics::Exception const& e) {
            if (e.code() == TRI_ERROR_REPLICATION_LEADER_CHANGE) {
              if (auto ptr = self->parent.lock(); ptr) {
                ptr->flush();
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

  void applyEntries(std::unique_ptr<Iterator> iter) noexcept {
    TRI_ASSERT(state != nullptr);
    TRI_ASSERT(iter != nullptr);
    auto range = iter->range();
    state->applyEntries(std::move(iter))
        .thenFinal([self = this->shared_from_this(), range](futures::Try<Result> tryResult) {
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
                << "follower failed to apply range " << range << " with unknown exception";
          }

          LOG_TOPIC("c89c8", DEBUG, Logger::REPLICATED_STATE)
              << "trigger retry for polling";
          // TODO retry
          std::abort();
        });
  }
};

template <typename S, typename Factory>
void ReplicatedState<S, Factory>::runFollower(std::shared_ptr<replicated_log::LogFollower> logFollower) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto machine = std::make_shared<FollowerState>(this->shared_from_this(), logFollower);
  machine->run();
  currentState = machine;
}

template <typename S, typename Factory>
void ReplicatedState<S, Factory>::runLeader(std::shared_ptr<replicated_log::LogLeader> logLeader) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE) << "create leader state";
  auto machine = std::make_shared<LeaderState>(this->shared_from_this(), logLeader);
  machine->run();
  currentState = machine;
}

template <typename S>
auto ReplicatedLeaderState<S>::getStream() const -> std::shared_ptr<Stream> const& {
  if (_stream) {
    return _stream;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
}

template <typename S>
auto ReplicatedFollowerState<S>::getStream() const -> std::shared_ptr<Stream> const& {
  if (_stream) {
    return _stream;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
}

template <typename S, typename Factory>
ReplicatedState<S, Factory>::ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                                             std::shared_ptr<Factory> factory)
    : log(std::move(log)), factory(std::move(factory)) {}
template <typename S, typename Factory>
void ReplicatedState<S, Factory>::flush() {
  auto participant = log->getParticipant();
  if (auto leader = std::dynamic_pointer_cast<replicated_log::LogLeader>(participant); leader) {
    runLeader(std::move(leader));
  } else if (auto follower = std::dynamic_pointer_cast<replicated_log::LogFollower>(participant);
             follower) {
    runFollower(std::move(follower));
  } else {
    // unconfigured
    std::abort();
  }
}

template <typename S, typename Factory>
auto ReplicatedState<S, Factory>::getFollower() const -> std::shared_ptr<FollowerType> {
  if (auto machine = std::dynamic_pointer_cast<FollowerState>(currentState); machine) {
    return std::static_pointer_cast<FollowerType>(machine->state);
  }
  return nullptr;
}

template <typename S, typename Factory>
auto ReplicatedState<S, Factory>::getLeader() const -> std::shared_ptr<LeaderType> {
  if (auto machine = std::dynamic_pointer_cast<LeaderState>(currentState); machine) {
    return std::static_pointer_cast<LeaderType>(machine->_machine);
  }
  return nullptr;
}

}  // namespace arangodb::replication2::replicated_state
