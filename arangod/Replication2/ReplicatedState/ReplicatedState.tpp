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

#include "Basics/DownCast.h"
#include "Inspection/VPack.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
auto IReplicatedLeaderState<S>::getStream() const noexcept
    -> std::shared_ptr<Stream> const& {
  ADB_PROD_ASSERT(_stream != nullptr)
      << "Replicated leader state: stream accessed before service was "
         "started.";

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
}

template<typename S>
auto IReplicatedFollowerState<S>::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  if (auto manager = _manager.lock(); manager != nullptr) {
    return manager->waitForApplied(index).thenValue(
        [](LogIndex) { return futures::Unit(); });
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
    std::shared_ptr<ReplicatedStateMetrics> metrics,
    std::shared_ptr<IScheduler> scheduler)
    : factory(std::move(factory)),
      gid(std::move(gid)),
      log(std::move(log)),
      loggerContext(std::move(loggerContext)),
      metrics(std::move(metrics)),
      scheduler(std::move(scheduler)) {
  TRI_ASSERT(this->metrics != nullptr);
  this->metrics->replicatedStateNumber->fetch_add(1);
}

template<typename S>
auto ReplicatedState<S>::getFollower() const -> std::shared_ptr<FollowerType> {
  ADB_PROD_ASSERT(manager.has_value());
  return basics::downCast<FollowerType>(manager->getFollower());
}

template<typename S>
auto ReplicatedState<S>::getLeader() const -> std::shared_ptr<LeaderType> {
  ADB_PROD_ASSERT(manager.has_value());
  return basics::downCast<LeaderType>(manager->getLeader());
}

template<typename S>
ReplicatedState<S>::~ReplicatedState() {
  metrics->replicatedStateNumber->fetch_sub(1);
}

template<typename S>
auto ReplicatedState<S>::buildCore(
    TRI_vocbase_t& vocbase,
    std::optional<velocypack::SharedSlice> const& coreParameter) {
  using CoreParameterType = typename S::CoreParameterType;
  if constexpr (std::is_void_v<CoreParameterType>) {
    return factory->constructCore(vocbase, gid);
  } else {
    if (!coreParameter.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Cannot find core parameter for replicated state with "
                      "ID {}, created in database {}, for {} state",
                      gid.id, gid.database, S::NAME));
    }
    auto params = CoreParameterType{};
    try {
      params =
          velocypack::deserialize<CoreParameterType>(coreParameter->slice());
    } catch (std::exception const& ex) {
      LOG_CTX("63857", ERR, loggerContext)
          << "failed to deserialize core parameters for replicated state "
          << gid << ". " << ex.what() << " json = " << coreParameter->toJson();
      throw;
    }
    return factory->constructCore(vocbase, gid, std::move(params));
  }
}

template<typename S>
void ReplicatedState<S>::drop(
    std::shared_ptr<replicated_log::IReplicatedStateHandle> stateHandle) && {
  ADB_PROD_ASSERT(stateHandle != nullptr);
  ADB_PROD_ASSERT(manager.has_value());
  auto core = std::move(*manager).resign();
  ADB_PROD_ASSERT(core != nullptr);

  using CleanupHandler = typename ReplicatedStateTraits<S>::CleanupHandlerType;
  if constexpr (not std::is_void_v<CleanupHandler>) {
    static_assert(
        std::is_invocable_r_v<std::shared_ptr<CleanupHandler>,
                              decltype(&Factory::constructCleanupHandler),
                              Factory>);
    std::shared_ptr<CleanupHandler> cleanupHandler =
        factory->constructCleanupHandler();
    cleanupHandler->drop(std::move(core));
  }
}

template<typename S>
auto ReplicatedState<S>::createStateHandle(
    TRI_vocbase_t& vocbase,
    std::optional<velocypack::SharedSlice> const& coreParameter)
    -> std::unique_ptr<replicated_log::IReplicatedStateHandle> {
  // TODO Should we make sure not to build the core twice?
  auto core = buildCore(vocbase, coreParameter);
  ADB_PROD_ASSERT(not manager.has_value());
  manager.emplace(loggerContext, metrics, std::move(core), factory, scheduler);

  struct Wrapper : replicated_log::IReplicatedStateHandle {
    explicit Wrapper(ReplicatedStateManager<S>& manager) : manager(manager) {}
    // MSVC chokes on trailing return type notation here
    [[nodiscard]] std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>
    resignCurrentState() noexcept override {
      return manager.resignCurrentState();
    }
    void leadershipEstablished(
        std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> ptr)
        override {
      return manager.leadershipEstablished(std::move(ptr));
    }
    void becomeFollower(
        std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> ptr)
        override {
      return manager.becomeFollower(std::move(ptr));
    }
    void acquireSnapshot(ServerID leader, LogIndex index,
                         std::uint64_t version) override {
      return manager.acquireSnapshot(std::move(leader), index, version);
    }
    void updateCommitIndex(LogIndex index) override {
      return manager.updateCommitIndex(index);
    }

    // MSVC chokes on trailing return type notation here
    [[nodiscard]] Status getInternalStatus() const override {
      return manager.getInternalStatus();
    }

    ReplicatedStateManager<S>& manager;
  };

  return std::make_unique<Wrapper>(*manager);
}

}  // namespace arangodb::replication2::replicated_state
