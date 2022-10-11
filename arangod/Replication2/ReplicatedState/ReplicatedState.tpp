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

#include "Metrics/Gauge.h"
#include "Replication2/ReplicatedLog/LogUnconfiguredParticipant.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/FollowerStateManager.h"
#include "Replication2/ReplicatedState/LazyDeserializingIterator.h"
#include "Replication2/ReplicatedState/LeaderStateManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/ReplicatedState/UnconfiguredStateManager.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"

#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/FollowerStateManager.tpp"
#include "Replication2/ReplicatedState/LeaderStateManager.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/ReplicatedState/UnconfiguredStateManager.tpp"
#include "Replication2/Streams/LogMultiplexer.tpp"
#include "Logger/LogContextKeys.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
ReplicatedStateManager<S>::ReplicatedStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::unique_ptr<CoreType> logCore, std::shared_ptr<Factory> factory)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _factory(std::move(factory)),
      _guarded{
          //
          std::in_place_type<NewUnconfiguredStateManager<S>>,  //
          _loggerContext.with<logContextKeyStateRole>(
              static_strings::StringUnconfigured),
          std::move(logCore)  //
      } {}

template<typename S>
void ReplicatedStateManager<S>::acquireSnapshot(ServerID leader,
                                                LogIndex commitIndex) {
  auto guard = _guarded.getLockedGuard();

  std::visit(overload{
                 [&](NewFollowerStateManager<S>& manager) {
                   LOG_CTX("52a11", DEBUG, _loggerContext)
                       << "try to acquire a new snapshot, starting at "
                       << commitIndex;
                   manager.acquireSnapshot(leader, commitIndex);
                 },
                 [](auto&&) {
                   ADB_PROD_ASSERT(false)
                       << "State is not a follower (or uninitialized), but "
                          "acquireSnapshot is called";
                 },
             },
             guard->_currentManager);
}

template<typename S>
void ReplicatedStateManager<S>::updateCommitIndex(LogIndex index) {
  auto guard = _guarded.getLockedGuard();

  std::visit(overload{
                 [index](auto& manager) { manager.updateCommitIndex(index); },
                 [](NewUnconfiguredStateManager<S>& manager) {
                   ADB_PROD_ASSERT(false) << "update commit index called on "
                                             "an unconfigured state manager";
                 },
             },
             guard->_currentManager);
}

template<typename S>
auto ReplicatedStateManager<S>::resign() noexcept
    -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> {
  auto guard = _guarded.getLockedGuard();
  auto&& [core, methods] =
      std::visit([](auto& manager) { return std::move(manager).resign(); },
                 guard->_currentManager);
  // TODO Is it allowed to happen that resign() is called on an unconfigured
  // state?
  ADB_PROD_ASSERT(std::holds_alternative<NewUnconfiguredStateManager<S>>(
                      guard->_currentManager) == (methods == nullptr));
  guard->_currentManager.template emplace<NewUnconfiguredStateManager<S>>(
      _loggerContext.template with<logContextKeyStateRole>(
          static_strings::StringUnconfigured),
      std::move(core));
  return std::move(methods);
}

template<typename S>
void ReplicatedStateManager<S>::leadershipEstablished(
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods) {
  auto guard = _guarded.getLockedGuard();
  ADB_PROD_ASSERT(std::holds_alternative<NewUnconfiguredStateManager<S>>(
      guard->_currentManager));
  auto&& [core, oldMethods] =
      std::move(
          std::get<NewUnconfiguredStateManager<S>>(guard->_currentManager))
          .resign();
  ADB_PROD_ASSERT(oldMethods == nullptr);
  auto leaderState = _factory->constructLeader(std::move(core));
  // TODO Pass the stream during construction already, and delete the
  //      "setStream" method; after that, the leader state implementation can
  //      also really rely on the stream being there.
  leaderState->setStream(std::make_shared<ProducerStreamProxy<EntryType>>());
  auto& manager =
      guard->_currentManager.template emplace<NewLeaderStateManager<S>>(
          _loggerContext.template with<logContextKeyStateRole>(
              static_strings::StringLeader),
          _metrics, std::move(leaderState), std::move(methods));

  manager.recoverEntries();
}

template<typename S>
void ReplicatedStateManager<S>::becomeFollower(
    std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> methods) {
  auto guard = _guarded.getLockedGuard();
  ADB_PROD_ASSERT(std::holds_alternative<NewUnconfiguredStateManager<S>>(
      guard->_currentManager));
  auto&& [core, oldMethods] =
      std::move(
          std::get<NewUnconfiguredStateManager<S>>(guard->_currentManager))
          .resign();
  ADB_PROD_ASSERT(oldMethods == nullptr);

  auto followerState = _factory->constructFollower(std::move(core));
  guard->_currentManager.template emplace<NewFollowerStateManager<S>>(
      _loggerContext.template with<logContextKeyStateRole>(
          static_strings::StringFollower),
      _metrics, std::move(followerState), std::move(methods));
}

template<typename S>
void ReplicatedStateManager<S>::dropEntries() {
  ADB_PROD_ASSERT(false);
}

template<typename S>
void NewLeaderStateManager<S>::recoverEntries() {
  auto future = _guardedData.getLockedGuard()->recoverEntries();
  // TODO
}

template<typename S>
auto NewLeaderStateManager<S>::GuardedData::recoverEntries() {
  auto logSnapshot = _logMethods->getLogSnapshot();
  auto logIter = logSnapshot.getIteratorFrom(LogIndex{0});
  auto deserializedIter =
      std::make_unique<LazyDeserializingIterator<EntryType, Deserializer>>(
          std::move(logIter));
  MeasureTimeGuard timeGuard(_metrics.replicatedStateRecoverEntriesRtt);
  auto fut = _leaderState->recoverEntries(std::move(deserializedIter))
                 .then([guard = std::move(timeGuard)](auto&& res) mutable {
                   guard.fire();
                   return std::move(res.get());
                 });
  return fut;
}

template<typename S>
void NewLeaderStateManager<S>::updateCommitIndex(LogIndex index) {
  // TODO post resolving waitFor promises onto the scheduler
}

template<typename S>
NewLeaderStateManager<S>::NewLeaderStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IReplicatedLeaderState<S>> leaderState,
    std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> logMethods)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _guardedData{_loggerContext, *_metrics, std::move(leaderState),
                   std::move(logMethods)} {}

template<typename S>
auto NewLeaderStateManager<S>::resign() noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  // TODO implement
  TRI_ASSERT(false);
}

template<typename S>
void NewFollowerStateManager<S>::updateCommitIndex(LogIndex commitIndex) {
  auto future = _guardedData.getLockedGuard()->updateCommitIndex(commitIndex);
  // TODO
}

template<typename S>
auto NewFollowerStateManager<S>::GuardedData::updateCommitIndex(
    LogIndex commitIndex) {
  auto log = _logMethods->getLogSnapshot();
  auto logIter = log.getIteratorRange(_lastAppliedIndex, commitIndex);
  auto deserializedIter = std::make_unique<
      LazyDeserializingIterator<EntryType const&, Deserializer>>(
      std::move(logIter));
  auto fut = _followerState->applyEntries(std::move(deserializedIter));
  // TODO update _lastAppliedIndex after applying entries
  // TODO handle concurrent calls to updateCommitIndex while applyEntries is
  //      running
  // get log iterator follower->applyEntries(iter); todo applyEntries
  // returns a future. Is this necessary?
  //      If yes, we have to deal with this async behaviour here.
  return fut;
}

template<typename S>
NewFollowerStateManager<S>::NewFollowerStateManager(
    LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IReplicatedFollowerState<S>> followerState,
    std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> logMethods)
    : _loggerContext(std::move(loggerContext)),
      _metrics(std::move(metrics)),
      _guardedData{std::move(followerState), std::move(logMethods)} {}

template<typename S>
void NewFollowerStateManager<S>::acquireSnapshot(ServerID leader,
                                                 LogIndex index) {
  // TODO implement
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template<typename S>
auto NewFollowerStateManager<S>::resign() noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  // TODO implement
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template<typename S>
NewUnconfiguredStateManager<S>::NewUnconfiguredStateManager(
    LoggerContext loggerContext, std::unique_ptr<CoreType> core) noexcept
    : _loggerContext(std::move(loggerContext)),
      _guardedData{GuardedData{._core = std::move(core)}} {}

template<typename S>
auto IReplicatedLeaderState<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> const& {
  ADB_PROD_ASSERT(_stream != nullptr)
      << "Replicated leader state: stream accessed before service was started.";

  return _stream;
}

template<typename S>
auto IReplicatedFollowerState<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> const& {
  ADB_PROD_ASSERT(_stream != nullptr) << "Replicated follower state: stream "
                                         "accessed before service was started.";

  return _stream;
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
    GlobalLogIdentifier gid, std::shared_ptr<replicated_log::ReplicatedLog> log,
    std::shared_ptr<Factory> factory, LoggerContext loggerContext,
    std::shared_ptr<ReplicatedStateMetrics> metrics)
    : factory(std::move(factory)),
      gid(std::move(gid)),
      log(std::move(log)),
      guardedData(*this),
      loggerContext(std::move(loggerContext)),
      metrics(std::move(metrics)) {
  TRI_ASSERT(this->metrics != nullptr);
  this->metrics->replicatedStateNumber->fetch_add(1);
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
ReplicatedState<S>::~ReplicatedState() {
  metrics->replicatedStateNumber->fetch_sub(1);
}

template<typename S>
auto ReplicatedState<S>::buildCore(
    std::optional<velocypack::SharedSlice> const& coreParameter) {
  if constexpr (std::is_void_v<typename S::CoreParameterType>) {
    return factory->constructCore(gid);
  } else {
    if (!coreParameter.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Cannot find core parameter for replicated state with "
                      "ID {}, created in database {}, for {} state",
                      gid.id, gid.database, S::NAME));
    }
    auto params = velocypack::deserialize<typename S::CoreParameterType>(
        coreParameter->slice());
    PersistedStateInfo info;
    info.stateId = log->getId();
    info.specification.type = S::NAME;
    info.specification.parameters = *coreParameter;
    return factory->constructCore(gid, std::move(params));
  }
}

template<typename S>
void ReplicatedState<S>::drop() && {
  auto deferred = guardedData.doUnderLock([&](GuardedData& data) {
    std::unique_ptr<CoreType> core;
    DeferredAction action;
    if (data.currentManager == nullptr) {
      core = std::move(data.oldCore);
    } else {
      std::tie(core, std::ignore, action) =
          std::move(*data.currentManager).resign();
    }

    using CleanupHandler =
        typename ReplicatedStateTraits<S>::CleanupHandlerType;
    if constexpr (not std::is_void_v<CleanupHandler>) {
      static_assert(
          std::is_invocable_r_v<std::shared_ptr<CleanupHandler>,
                                decltype(&Factory::constructCleanupHandler),
                                Factory>);
      std::shared_ptr<CleanupHandler> cleanupHandler =
          factory->constructCleanupHandler();
      cleanupHandler->drop(std::move(core));
    }
    return action;
  });
  deferred.fire();
}

template<typename S>
auto ReplicatedState<S>::createStateHandle()
    -> std::unique_ptr<replicated_log::IReplicatedStateHandle> {
  // TODO Should we make sure not to build the core twice?
  // TODO Fix or remove the parameter argument
  auto core = buildCore({});
  auto handle = std::make_unique<ReplicatedStateManager<S>>(
      loggerContext, metrics, std::move(core), factory);

  return handle;
}

}  // namespace arangodb::replication2::replicated_state
