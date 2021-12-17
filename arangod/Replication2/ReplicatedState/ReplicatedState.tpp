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
#include "Replication2/ReplicatedState/LeaderStateManager.h"
#include "Replication2/ReplicatedState/FollowerStateManager.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"

#include "Replication2/Streams/LogMultiplexer.tpp"
#include "Replication2/ReplicatedState/LeaderStateManager.tpp"
#include "Replication2/ReplicatedState/FollowerStateManager.tpp"

namespace arangodb::replication2::replicated_state {

template<typename S>
void ReplicatedState<S>::runFollower(
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<ReplicatedStateCore> core) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto manager = std::make_shared<FollowerStateManager<S>>(this->shared_from_this(),
                                                 logFollower, std::move(core));
  manager->run();
  currentManager = manager;
}

template<typename S>
void ReplicatedState<S>::runLeader(
    std::shared_ptr<replicated_log::ILogLeader> logLeader,
    std::unique_ptr<ReplicatedStateCore> core) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE) << "create leader state";
  auto manager = std::make_shared<LeaderStateManager<S>>(this->shared_from_this(),
                                               logLeader, std::move(core));
  manager->run();
  currentManager = manager;
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
    : factory(std::move(factory)), log(std::move(log)) {}

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
  if (auto machine = std::dynamic_pointer_cast<FollowerStateManager<S>>(currentManager);
      machine) {
    return std::static_pointer_cast<FollowerType>(machine->state);
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getLeader() const -> std::shared_ptr<LeaderType> {
  if (auto internalState = std::dynamic_pointer_cast<LeaderStateManager<S>>(currentManager);
      internalState) {
    if (internalState->state != nullptr) {
      return std::static_pointer_cast<LeaderType>(internalState->state);
    }
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getStatus() -> StateStatus {
  return currentManager->getStatus();
}

template<typename S>
auto ReplicatedState<S>::getSnapshotStatus() const -> SnapshotStatus {
  return currentManager->getSnapshotStatus();
}

}  // namespace arangodb::replication2::replicated_state
