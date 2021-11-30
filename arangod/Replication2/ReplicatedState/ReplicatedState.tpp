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

  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  void run() {
    // 1. wait for leadership established
    // 1.2. digest available entries into multiplexer
    // 2. construct leader state
    // 2.2 apply all log entries of the previous term
    // 3. make leader state available
    // 2. on configuration change, update
    LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
        << "LeaderState waiting for leadership to be established";
    _leader->waitForLeadership()
        .thenValue([self = this->shared_from_this()](auto&& result) {
          LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
              << "LeaderState established";
          self->_mux = Multiplexer::construct(self->_leader);
          self->_mux->digestAvailableEntries();

          auto stream = self->_mux->template getStreamById<1>();
          LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
              << "receiving committed entries for recovery";
          // TODO we don't have to `waitFor` we can just access the log.
          //    new entries are not yet written, because the stream was
          //    not published.
          return stream->waitForIterator(LogIndex{0}).thenValue([self](std::unique_ptr<Iterator>&& result) {
            LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                << "starting recovery";
            if (auto parent = self->_parent.lock(); parent) {
              self->_machine = parent->factory->constructLeader();
              return self->_machine->recoverEntries(std::move(result));
            }
            return futures::Future<Result>{TRI_ERROR_REPLICATION_LEADER_CHANGE};
          });
        })
        .thenValue([self = this->shared_from_this()](Result&& result) {
          LOG_TOPIC("fb593", TRACE, Logger::REPLICATED_STATE)
              << "recovery completed";
          self->_machine->_stream = self->_mux->template getStreamById<1>();
          LOG_TOPIC("fb593", TRACE, Logger::REPLICATED_STATE)
              << "starting leader service";
        });
  }

  using Multiplexer = streams::LogMultiplexer<ReplicatedStateStreamSpec<S>>;

  std::shared_ptr<ReplicatedLeaderState<S>> _machine;
  std::shared_ptr<Multiplexer> _mux;
  std::weak_ptr<ReplicatedState> _parent;
  std::shared_ptr<replicated_log::LogLeader> _leader;
};

template <typename S, typename F>
struct ReplicatedState<S, F>::FollowerState
    : StateBase,
      std::enable_shared_from_this<FollowerState> {
  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  LogIndex _nextEntry{0};
  FollowerState(std::shared_ptr<Stream> stream,
                std::shared_ptr<ReplicatedFollowerState<S>> state,
                std::weak_ptr<ReplicatedState> parent)
      : stream(std::move(stream)), state(std::move(state)), parent(std::move(parent)) {}

  std::shared_ptr<Stream> stream;
  std::shared_ptr<ReplicatedFollowerState<S>> state;
  std::weak_ptr<ReplicatedState> parent;

  void run() {
    state->_stream = stream;
    runNext();
  }

  void runNext() {
    LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
        << "FollowerState wait for iterator starting at " << _nextEntry;
    stream->waitForIterator(_nextEntry)
        .thenValue([this](std::unique_ptr<Iterator> iter) {
          auto [from, to] = iter->range();
          LOG_TOPIC("6626f", TRACE, Logger::REPLICATED_STATE)
              << "State machine received log range " << iter->range();
          // call into state machine to apply entries
          return state->applyEntries(std::move(iter)).thenValue([to = to](Result&& res) {
            return std::make_pair(to, res);
          });
        })
        .thenFinal([self = this->shared_from_this()](
                       futures::Try<std::pair<LogIndex, Result>>&& tryResult) {
          try {
            auto& [index, res] = tryResult.get();
            if (res.ok()) {
              self->_nextEntry = index;  // commit that index
              self->stream->release(index.saturatedDecrement());
              LOG_TOPIC("ab737", TRACE, Logger::REPLICATED_STATE)
                  << "State machine accepted log entries, next entry is " << index;
            } else {
              LOG_TOPIC("e8777", ERR, Logger::REPLICATED_STATE)
                  << "Follower state machine returned result " << res;
            }
            self->run();
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
              LOG_TOPIC("e73bc", ERR, Logger::REPLICATED_STATE)
                  << "Unexpected exception in applyEntries: " << e.what();
            }
          } catch (std::exception const& e) {
            LOG_TOPIC("e73bc", ERR, Logger::REPLICATED_STATE)
                << "Unexpected exception in applyEntries: " << e.what();
          } catch (...) {
            LOG_TOPIC("4d2b7", ERR, Logger::REPLICATED_STATE)
                << "Unexpected exception";
          }
        });
  }
};

template <typename S, typename Factory>
void ReplicatedState<S, Factory>::runFollower(std::shared_ptr<replicated_log::LogFollower> logFollower) {
  // 1. construct state follower
  // 2. wait for new entries, call applyEntries
  // 3. forward release index after the entries have been applied
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto state = factory->constructFollower();
  auto demux = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>::construct(logFollower);
  auto stream = demux->template getStreamById<1>();
  auto machine = std::make_shared<FollowerState>(stream, state, this->weak_from_this());
  machine->run();
  demux->listen();
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
