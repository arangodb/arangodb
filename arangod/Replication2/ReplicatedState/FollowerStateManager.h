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

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/ReplicatedState/StreamProxy.h"
#include "Replication2/ReplicatedState/WaitForQueue.h"

#include "Basics/Guarded.h"

namespace arangodb {
class Result;

namespace futures {
template<typename T>
class Future;
struct Unit;
}  // namespace futures

}  // namespace arangodb

namespace arangodb::replication2 {
struct IScheduler;

namespace replicated_state {

struct ReplicatedStateMetrics;

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

}  // namespace replicated_state
}  // namespace arangodb::replication2
