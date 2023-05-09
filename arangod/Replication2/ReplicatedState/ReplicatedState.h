////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Replication2/DeferredExecution.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/ReplicatedState/WaitForQueue.h"
#include "Replication2/Streams/Streams.h"
#include "Replication2/IScheduler.h"

#include "Basics/Guarded.h"

#include <map>

struct TRI_vocbase_t;

namespace arangodb {
class Result;

namespace futures {
template<typename T>
class Future;
template<typename T>
class Promise;
struct Unit;
}  // namespace futures

namespace velocypack {
class SharedSlice;
}

}  // namespace arangodb

namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
struct ILogFollower;
struct ILogLeader;
struct ILogParticipant;
}  // namespace replicated_log

namespace replicated_state {
struct ReplicatedStateMetrics;
struct IReplicatedLeaderStateBase;
struct IReplicatedFollowerStateBase;

template<typename S>
struct IReplicatedStateImplBase;
template<typename S>
struct IReplicatedFollowerState;
template<typename S>
struct IReplicatedLeaderState;

template<typename S>
using ReplicatedStateStreamSpec =
    streams::stream_descriptor_set<streams::stream_descriptor<
        streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
        streams::tag_descriptor_set<streams::tag_descriptor<
            1, typename ReplicatedStateTraits<S>::Deserializer,
            typename ReplicatedStateTraits<S>::Serializer>>>>;

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void drop(
      std::shared_ptr<replicated_log::IReplicatedStateHandle>) && = 0;
  [[nodiscard]] auto getLeader()
      -> std::shared_ptr<IReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  [[nodiscard]] auto getFollower()
      -> std::shared_ptr<IReplicatedFollowerStateBase> {
    return getFollowerBase();
  }

  [[nodiscard]] virtual auto createStateHandle(
      TRI_vocbase_t& vocbase,
      std::optional<velocypack::SharedSlice> const& coreParameters)
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> = 0;

 private:
  [[nodiscard]] virtual auto getLeaderBase()
      -> std::shared_ptr<IReplicatedLeaderStateBase> = 0;
  [[nodiscard]] virtual auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> = 0;
};

// The streams for follower states use IReplicatedLogFollowerMethods, while the
// leader ones use IReplicatedLogLeaderMethods.
template<class ILogMethodsT>
concept ValidStreamLogMethods =
    std::is_same_v<ILogMethodsT,
                   replicated_log::IReplicatedLogFollowerMethods> ||
    std::is_same_v<ILogMethodsT, replicated_log::IReplicatedLogLeaderMethods>;

// TODO Clean this up, starting with trimming Stream to its minimum
template<typename S, template<typename> typename Interface = streams::Stream,
         ValidStreamLogMethods ILogMethodsT =
             replicated_log::IReplicatedLogFollowerMethods>
struct StreamProxy : Interface<typename ReplicatedStateTraits<S>::EntryType> {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using WaitForResult = typename streams::Stream<EntryType>::WaitForResult;
  using Iterator = typename streams::Stream<EntryType>::Iterator;

 protected:
  Guarded<std::unique_ptr<ILogMethodsT>> _logMethods;

 public:
  explicit StreamProxy(std::unique_ptr<ILogMethodsT> methods);

  struct MethodsGuard {
    using Guard =
        typename Guarded<std::unique_ptr<ILogMethodsT>>::mutex_guard_type;
    auto operator->() noexcept -> ILogMethodsT* { return guard.get().get(); }
    auto operator*() noexcept -> ILogMethodsT& { return *guard.get().get(); }
    explicit MethodsGuard(Guard&& guard) : guard(std::move(guard)) {}
    auto isResigned() const noexcept -> bool { return guard.get() == nullptr; }

   private:
    Guard guard;
  };

  auto methods() -> MethodsGuard;

  auto resign() && -> std::unique_ptr<ILogMethodsT>;

  auto isResigned() const noexcept -> bool;

  auto waitFor(LogIndex index) -> futures::Future<WaitForResult> override;
  auto waitForIterator(LogIndex index)
      -> futures::Future<std::unique_ptr<Iterator>> override;

  auto release(LogIndex index) -> void override;

 protected:
  [[noreturn]] static void throwResignedException();
};

template<typename S>
struct ProducerStreamProxy
    : StreamProxy<S, streams::ProducerStream,
                  replicated_log::IReplicatedLogLeaderMethods> {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  explicit ProducerStreamProxy(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> methods);

  // TODO waitForSync parameter is missing
  auto insert(EntryType const& v) -> LogIndex override;

 private:
  auto serialize(EntryType const& v) -> LogPayload;
};

template<typename S>
struct LeaderStateManager
    : std::enable_shared_from_this<LeaderStateManager<S>> {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  using StreamImpl = ProducerStreamProxy<S>;

  explicit LeaderStateManager(
      LoggerContext loggerContext,
      std::shared_ptr<ReplicatedStateMetrics> metrics,
      std::shared_ptr<IReplicatedLeaderState<S>> leaderState,
      std::shared_ptr<StreamImpl> stream);

  void recoverEntries();
  void updateCommitIndex(LogIndex index) noexcept {
    // TODO do we have to do anything?
  }
  [[nodiscard]] auto resign() && noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;
  [[nodiscard]] auto getInternalStatus() const -> Status::Leader;

  [[nodiscard]] auto getStateMachine() const
      -> std::shared_ptr<IReplicatedLeaderState<S>>;

 private:
  LoggerContext const _loggerContext;
  std::shared_ptr<ReplicatedStateMetrics> const _metrics;
  struct GuardedData {
    auto recoverEntries();
    [[nodiscard]] auto resign() && noexcept -> std::pair<
        std::unique_ptr<CoreType>,
        std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;

    LoggerContext const& _loggerContext;
    ReplicatedStateMetrics const& _metrics;
    std::shared_ptr<IReplicatedLeaderState<S>> _leaderState;
    std::shared_ptr<StreamImpl> _stream;
    bool _recoveryCompleted{false};
  };
  Guarded<GuardedData> _guardedData;
};

template<typename S>
struct FollowerStateManager
    : std::enable_shared_from_this<FollowerStateManager<S>> {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;

  using StreamImpl = StreamProxy<S>;

  explicit FollowerStateManager(
      LoggerContext loggerContext,
      std::shared_ptr<ReplicatedStateMetrics> metrics,
      std::shared_ptr<IReplicatedFollowerState<S>> followerState,
      std::shared_ptr<StreamImpl> stream,
      std::shared_ptr<IScheduler> scheduler);
  void acquireSnapshot(ServerID leader, LogIndex index, std::uint64_t);
  void updateCommitIndex(LogIndex index);
  [[nodiscard]] auto resign() && noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;
  [[nodiscard]] auto getInternalStatus() const -> Status::Follower;

  [[nodiscard]] auto getStateMachine() const
      -> std::shared_ptr<IReplicatedFollowerState<S>>;
  [[nodiscard]] auto waitForApplied(LogIndex) -> WaitForQueue::WaitForFuture;

 private:
  LoggerContext const _loggerContext;
  std::shared_ptr<ReplicatedStateMetrics> const _metrics;
  std::shared_ptr<IScheduler> const _scheduler;
  void handleApplyEntriesResult(Result);
  auto backOffSnapshotRetry() -> futures::Future<futures::Unit>;
  void registerSnapshotError(Result error) noexcept;
  struct GuardedData {
    [[nodiscard]] auto updateCommitIndex(
        LogIndex index, std::shared_ptr<ReplicatedStateMetrics> const& metrics,
        std::shared_ptr<IScheduler> const& scheduler)
        -> std::optional<futures::Future<Result>>;
    [[nodiscard]] auto maybeScheduleApplyEntries(
        std::shared_ptr<ReplicatedStateMetrics> const& metrics,
        std::shared_ptr<IScheduler> const& scheduler)
        -> std::optional<futures::Future<Result>>;
    [[nodiscard]] auto getResolvablePromises(LogIndex index) noexcept
        -> WaitForQueue;
    [[nodiscard]] auto waitForApplied(LogIndex) -> WaitForQueue::WaitForFuture;
    void registerSnapshotError(Result error,
                               metrics::Counter& counter) noexcept;
    void clearSnapshotErrors() noexcept;

    std::shared_ptr<IReplicatedFollowerState<S>> _followerState;
    std::shared_ptr<StreamImpl> _stream;
    WaitForQueue _waitQueue{};
    LogIndex _commitIndex = LogIndex{0};
    LogIndex _lastAppliedIndex = LogIndex{0};
    std::optional<Result> _lastSnapshotError{};
    std::uint64_t _snapshotErrorCounter{0};
    std::optional<LogIndex> _applyEntriesIndexInFlight = std::nullopt;
  };
  Guarded<GuardedData> _guardedData;
};

template<typename S>
struct UnconfiguredStateManager
    : std::enable_shared_from_this<UnconfiguredStateManager<S>> {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  explicit UnconfiguredStateManager(LoggerContext loggerContext,
                                    std::unique_ptr<CoreType>) noexcept;
  [[nodiscard]] auto resign() && noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;
  [[nodiscard]] auto getInternalStatus() const -> Status::Unconfigured;

 private:
  LoggerContext const _loggerContext;
  struct GuardedData {
    [[nodiscard]] auto resign() && noexcept -> std::unique_ptr<CoreType>;

    std::unique_ptr<CoreType> _core;
  };
  Guarded<GuardedData> _guardedData;
};

template<typename S>
struct ReplicatedStateManager : replicated_log::IReplicatedStateHandle {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;

  ReplicatedStateManager(LoggerContext loggerContext,
                         std::shared_ptr<ReplicatedStateMetrics> metrics,
                         std::unique_ptr<CoreType> logCore,
                         std::shared_ptr<Factory> factory,
                         std::shared_ptr<IScheduler> scheduler);

  void acquireSnapshot(ServerID leader, LogIndex index,
                       std::uint64_t version) override;

  void updateCommitIndex(LogIndex index) override;

  [[nodiscard]] auto resignCurrentState() noexcept
      -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> override;

  void leadershipEstablished(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods>) override;

  void becomeFollower(
      std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> methods)
      override;

  auto resign() && -> std::unique_ptr<CoreType>;

  [[nodiscard]] auto getInternalStatus() const -> Status override;
  // We could, more specifically, return pointers to FollowerType/LeaderType.
  // But I currently don't see that it's needed, and would have to do one of
  // the stunts for covariance here.
  [[nodiscard]] auto getFollower() const
      -> std::shared_ptr<IReplicatedFollowerStateBase>;
  [[nodiscard]] auto getLeader() const
      -> std::shared_ptr<IReplicatedLeaderStateBase>;

 private:
  LoggerContext const _loggerContext;
  std::shared_ptr<ReplicatedStateMetrics> const _metrics;
  std::shared_ptr<Factory> const _factory;
  std::shared_ptr<IScheduler> const _scheduler;

  struct GuardedData {
    template<typename... Args>
    explicit GuardedData(Args&&... args)
        : _currentManager(std::forward<Args>(args)...) {}

    std::variant<std::shared_ptr<UnconfiguredStateManager<S>>,
                 std::shared_ptr<LeaderStateManager<S>>,
                 std::shared_ptr<FollowerStateManager<S>>>
        _currentManager;
  };
  Guarded<GuardedData> _guarded;
};

template<typename S>
struct ReplicatedState final
    : ReplicatedStateBase,
      std::enable_shared_from_this<ReplicatedState<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;

  explicit ReplicatedState(GlobalLogIdentifier gid,
                           std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory,
                           LoggerContext loggerContext,
                           std::shared_ptr<ReplicatedStateMetrics> metrics,
                           std::shared_ptr<IScheduler> scheduler);
  ~ReplicatedState() override;

  void drop(std::shared_ptr<replicated_log::IReplicatedStateHandle>) &&
      override;
  /**
   * Returns the follower state machine. Returns nullptr if no follower state
   * machine is present. (i.e. this server is not a follower)
   */
  [[nodiscard]] auto getFollower() const -> std::shared_ptr<FollowerType>;
  /**
   * Returns the leader state machine. Returns nullptr if no leader state
   * machine is present. (i.e. this server is not a leader)
   */
  [[nodiscard]] auto getLeader() const -> std::shared_ptr<LeaderType>;

  auto createStateHandle(
      TRI_vocbase_t& vocbase,
      std::optional<velocypack::SharedSlice> const& coreParameter)
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> override;

 private:
  auto buildCore(TRI_vocbase_t& vocbase,
                 std::optional<velocypack::SharedSlice> const& coreParameter);
  auto getLeaderBase() -> std::shared_ptr<IReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> final {
    return getFollower();
  }

  std::optional<ReplicatedStateManager<S>> manager;

  std::shared_ptr<Factory> const factory;
  GlobalLogIdentifier const gid;
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};

  LoggerContext const loggerContext;
  DatabaseID const database;
  std::shared_ptr<ReplicatedStateMetrics> const metrics;
  std::shared_ptr<IScheduler> const scheduler;
};

}  // namespace replicated_state
}  // namespace arangodb::replication2
