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
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/LeaderStateManager.tpp"
#include "Replication2/ReplicatedState/FollowerStateManager.tpp"

namespace arangodb::replication2::replicated_state {

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
    : factory(std::move(factory)), log(std::move(log)), guardedData(*this) {}

template<typename S>
void ReplicatedState<S>::flush(StateGeneration planGeneration) {
  auto action = guardedData.getLockedGuard()->flush(planGeneration);
  (void)action;
}

template<typename S>
auto ReplicatedState<S>::getFollower() const -> std::shared_ptr<FollowerType> {
  auto guard = guardedData.getLockedGuard();
  if (auto machine = std::dynamic_pointer_cast<FollowerStateManager<S>>(
          guard->currentManager);
      machine) {
    return std::static_pointer_cast<FollowerType>(machine->getFollowerState());
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getLeader() const -> std::shared_ptr<LeaderType> {
  auto guard = guardedData.getLockedGuard();
  if (auto internalState = std::dynamic_pointer_cast<LeaderStateManager<S>>(
          guard->currentManager);
      internalState) {
    if (internalState->state != nullptr) {
      return std::static_pointer_cast<LeaderType>(internalState->state);
    }
  }
  return nullptr;
}

template<typename S>
auto ReplicatedState<S>::getStatus() -> std::optional<StateStatus> {
  return guardedData.doUnderLock(
      [&](GuardedData& data) -> std::optional<StateStatus> {
        if (data.currentManager == nullptr) {
          return std::nullopt;
        }
        return data.currentManager->getStatus();
      });
}

template<typename S>
void ReplicatedState<S>::forceRebuild() {
  LOG_TOPIC("8041a", INFO, Logger::REPLICATED_STATE)
      << "Force rebuild of replicated state";
  auto action = guardedData.getLockedGuard()->forceRebuild();
  (void)action;
}

template<typename S>
void ReplicatedState<S>::start(std::unique_ptr<ReplicatedStateToken> token) {
  auto core = std::make_unique<ReplicatedStateCore>();
  auto action =
      guardedData.getLockedGuard()->rebuild(std::move(core), std::move(token));
  (void)action;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::rebuild(
    std::unique_ptr<ReplicatedStateCore> core,
    std::unique_ptr<ReplicatedStateToken> token) -> DeferredAction try {
  LOG_TOPIC("edaef", TRACE, Logger::REPLICATED_STATE)
      << "replicated state rebuilding - query participant";
  auto participant = _self.log->getParticipant();
  if (auto leader =
          std::dynamic_pointer_cast<replicated_log::ILogLeader>(participant);
      leader) {
    LOG_TOPIC("99890", TRACE, Logger::REPLICATED_STATE)
        << "obtained leader participant";
    return runLeader(std::move(leader), std::move(core), std::move(token));
  } else if (auto follower =
                 std::dynamic_pointer_cast<replicated_log::ILogFollower>(
                     participant);
             follower) {
    LOG_TOPIC("f5328", TRACE, Logger::REPLICATED_STATE)
        << "obtained follower participant";
    return runFollower(std::move(follower), std::move(core), std::move(token));
  } else {
    // unconfigured
    std::abort();
  }
} catch (basics::Exception const& ex) {
  if (ex.code() == TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE) {
    LOG_TOPIC("eacb9", TRACE, Logger::REPLICATED_STATE)
        << "Replicated log participant is gone. Replicated state will go soon "
           "as well.";
    return {};
  }
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::runFollower(
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<ReplicatedStateCore> core,
    std::unique_ptr<ReplicatedStateToken> token) -> DeferredAction try {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto manager = std::make_shared<FollowerStateManager<S>>(
      _self.shared_from_this(), logFollower, std::move(core), std::move(token),
      _self.factory);
  currentManager = manager;

  return DeferredAction{[manager]() noexcept { manager->run(); }};
} catch (std::exception const& e) {
  LOG_DEVEL << "runFollower caught exception: " << e.what();
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::runLeader(
    std::shared_ptr<replicated_log::ILogLeader> logLeader,
    std::unique_ptr<ReplicatedStateCore> core,
    std::unique_ptr<ReplicatedStateToken> token) -> DeferredAction try {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE) << "create leader state";
  auto manager = std::make_shared<LeaderStateManager<S>>(
      _self.shared_from_this(), logLeader, std::move(core), std::move(token),
      _self.factory);
  currentManager = manager;

  return DeferredAction{[manager]() noexcept { manager->run(); }};
} catch (std::exception const& e) {
  LOG_DEVEL << "runFollower caught exception: " << e.what();
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::forceRebuild() -> DeferredAction {
  try {
    auto [core, token] = std::move(*currentManager).resign();
    return rebuild(std::move(core), std::move(token));
  } catch (std::exception const& e) {
    LOG_DEVEL << "forceRebuild caught exception: " << e.what();
    throw;
  }
}

template<typename S>
auto ReplicatedState<S>::GuardedData::flush(StateGeneration planGeneration)
    -> DeferredAction {
  auto [core, token] = std::move(*currentManager).resign();
  if (token->generation != planGeneration) {
    token = std::make_unique<ReplicatedStateToken>(planGeneration);
  }

  return rebuild(std::move(core), std::move(token));
}

}  // namespace arangodb::replication2::replicated_state
