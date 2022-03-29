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

#include "Replication2/ReplicatedLog/LogUnconfiguredParticipant.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/LeaderStateManager.h"
#include "Replication2/ReplicatedState/FollowerStateManager.h"
#include "Replication2/ReplicatedState/UnconfiguredStateManager.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"

#include "Replication2/Streams/LogMultiplexer.tpp"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/LeaderStateManager.tpp"
#include "Replication2/ReplicatedState/FollowerStateManager.tpp"
#include "Replication2/ReplicatedState/UnconfiguredStateManager.tpp"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Logger/LogContextKeys.h"

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
void IReplicatedFollowerState<S>::setStateManager(
    std::shared_ptr<FollowerStateManager<S>> manager) noexcept {
  _manager = manager;
  _stream = manager->getStream();
  TRI_ASSERT(_stream != nullptr);
}

template<typename S>
auto IReplicatedFollowerState<S>::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  if (auto manager = _manager.lock(); manager != nullptr) {
    return manager->waitForApplied(index);
  } else {
    WaitForAppliedFuture future(
        std::make_exception_ptr(replicated_log::ParticipantResignedException(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE)));
    return future;
  }
}

template<typename S>
ReplicatedState<S>::ReplicatedState(
    std::shared_ptr<replicated_log::ReplicatedLog> log,
    std::shared_ptr<Factory> factory, LoggerContext loggerContext)
    : factory(std::move(factory)),
      log(std::move(log)),
      guardedData(*this),
      loggerContext(std::move(loggerContext)) {}

template<typename S>
void ReplicatedState<S>::flush(StateGeneration planGeneration) {
  auto deferred = guardedData.getLockedGuard()->flush(planGeneration);
  // execute *after* the lock has been released
  deferred.fire();
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
    if (auto state = internalState->getImplementationState();
        state != nullptr) {
      return std::static_pointer_cast<LeaderType>(state);
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
        // This is guaranteed to not throw in case the currentManager is not
        // resigned.
        return data.currentManager->getStatus();
      });
}

template<typename S>
void ReplicatedState<S>::forceRebuild() {
  LOG_CTX("8041a", TRACE, loggerContext) << "Force rebuild of replicated state";
  auto deferred = guardedData.getLockedGuard()->forceRebuild();
  // execute *after* the lock has been released
  deferred.fire();
}

template<typename S>
void ReplicatedState<S>::start(std::unique_ptr<ReplicatedStateToken> token) {
  auto core = factory->constructCore(log->getGlobalLogId());
  auto deferred =
      guardedData.getLockedGuard()->rebuild(std::move(core), std::move(token));
  // execute *after* the lock has been released
  deferred.fire();
}

template<typename S>
auto ReplicatedState<S>::GuardedData::rebuild(
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    -> DeferredAction try {
  LOG_CTX("edaef", TRACE, _self.loggerContext)
      << "replicated state rebuilding - query participant";
  auto participant = _self.log->getParticipant();
  if (auto leader =
          std::dynamic_pointer_cast<replicated_log::ILogLeader>(participant);
      leader) {
    LOG_CTX("99890", TRACE, _self.loggerContext)
        << "obtained leader participant";
    return runLeader(std::move(leader), std::move(core), std::move(token));
  } else if (auto follower =
                 std::dynamic_pointer_cast<replicated_log::ILogFollower>(
                     participant);
             follower) {
    LOG_CTX("f5328", TRACE, _self.loggerContext)
        << "obtained follower participant";
    return runFollower(std::move(follower), std::move(core), std::move(token));
  } else if (auto unconfiguredLogParticipant = std::dynamic_pointer_cast<
                 replicated_log::LogUnconfiguredParticipant>(participant);
             unconfiguredLogParticipant) {
    LOG_CTX("ad84b", TRACE, _self.loggerContext)
        << "obtained unconfigured participant";
    return runUnconfigured(std::move(unconfiguredLogParticipant),
                           std::move(core), std::move(token));
  } else {
    LOG_CTX("33d5f", FATAL, _self.loggerContext)
        << "Replicated log has an unhandled participant type.";
    std::abort();
  }
} catch (basics::Exception const& ex) {
  if (ex.code() == TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE) {
    LOG_CTX("eacb9", TRACE, _self.loggerContext)
        << "Replicated log participant is gone. Replicated state will go soon "
           "as well.";
    return {};
  }
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::runFollower(
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    -> DeferredAction try {
  LOG_CTX("95b9d", DEBUG, _self.loggerContext) << "create follower state";

  auto loggerCtx =
      _self.loggerContext.template with<logContextKeyStateComponent>(
          "follower-manager");
  auto manager = std::make_shared<FollowerStateManager<S>>(
      std::move(loggerCtx), _self.shared_from_this(), std::move(logFollower),
      std::move(core), std::move(token), _self.factory);
  currentManager = manager;

  return DeferredAction([manager]() noexcept { manager->run(); });
} catch (std::exception const& e) {
  LOG_CTX("ab9de", DEBUG, _self.loggerContext)
      << "runFollower caught exception: " << e.what();
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::runLeader(
    std::shared_ptr<replicated_log::ILogLeader> logLeader,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    -> DeferredAction try {
  LOG_CTX("95b9d", DEBUG, _self.loggerContext) << "create leader state";

  auto loggerCtx =
      _self.loggerContext.template with<logContextKeyStateComponent>(
          "leader-manager");
  auto manager = std::make_shared<LeaderStateManager<S>>(
      std::move(loggerCtx), _self.shared_from_this(), std::move(logLeader),
      std::move(core), std::move(token), _self.factory);
  currentManager = manager;

  return DeferredAction([manager]() noexcept { manager->run(); });
} catch (std::exception const& e) {
  LOG_CTX("016f3", DEBUG, _self.loggerContext)
      << "run leader caught exception: " << e.what();
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::runUnconfigured(
    std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
        unconfiguredParticipant,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    -> DeferredAction try {
  LOG_CTX("5d7c6", DEBUG, _self.loggerContext) << "create unconfigured state";
  auto manager = std::make_shared<UnconfiguredStateManager<S>>(
      _self.shared_from_this(), std::move(unconfiguredParticipant),
      std::move(core), std::move(token));
  currentManager = manager;

  return DeferredAction([manager]() noexcept { manager->run(); });
} catch (std::exception const& e) {
  LOG_CTX("6f1eb", DEBUG, _self.loggerContext)
      << "run unconfigured caught exception: " << e.what();
  throw;
}

template<typename S>
auto ReplicatedState<S>::GuardedData::forceRebuild() -> DeferredAction {
  try {
    auto [core, token, queueAction] = std::move(*currentManager).resign();
    auto runAction = rebuild(std::move(core), std::move(token));
    return DeferredAction::combine(std::move(queueAction),
                                   std::move(runAction));
  } catch (std::exception const& e) {
    LOG_CTX("af348", DEBUG, _self.loggerContext)
        << "forced rebuild caught exception: " << e.what();
    throw;
  }
}

template<typename S>
auto ReplicatedState<S>::GuardedData::flush(StateGeneration planGeneration)
    -> DeferredAction {
  auto [core, token, queueAction] = std::move(*currentManager).resign();
  if (token->generation != planGeneration) {
    token = std::make_unique<ReplicatedStateToken>(planGeneration);
  }

  auto runAction = rebuild(std::move(core), std::move(token));
  return DeferredAction::combine(std::move(queueAction), std::move(runAction));
}

}  // namespace arangodb::replication2::replicated_state
