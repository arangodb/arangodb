////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Basics/Guarded.h"
#include "Replication2/DeferredExecution.h"
#include "Replication2/ReplicatedLog/Components/ICompactionManager.h"
#include "Replication2/LoggerContext.h"

namespace arangodb {
template<typename T>
class ResultT;
namespace futures {
template<typename T>
class Future;
template<typename T>
class Promise;
struct Unit;
}  // namespace futures

namespace replication2::replicated_log {
inline namespace comp {

struct IStorageManager;

template<typename T>
struct ResolveAggregator {
  auto waitFor() -> futures::Future<T> {
    return promises.emplace_back().getFuture();
  }

  auto resolveAll(futures::Try<T> result) -> DeferredAction {
    struct ResolveData {
      ContainerType promises;
      futures::Try<T> result;
      ResolveData(ContainerType promises, futures::Try<T> result)
          : promises(std::move(promises)), result(std::move(result)) {}
    };
    return DeferredAction{
        [data = std::make_unique<ResolveData>(
             std::move(promises), std::move(result))]() mutable noexcept {
          for (auto& p : data->promises) {
            auto copy = data->result;
            p.setTry(std::move(copy));
          }
        }};
  }

  auto empty() const noexcept { return promises.empty(); }
  auto size() const noexcept { return promises.size(); }

  ResolveAggregator() noexcept = default;
  ResolveAggregator(ResolveAggregator const&) = delete;
  ResolveAggregator(ResolveAggregator&&) noexcept = default;
  auto operator=(ResolveAggregator const&) -> ResolveAggregator& = delete;
  auto operator=(ResolveAggregator&&) noexcept -> ResolveAggregator& = default;

 private:
  using ContainerType = std::vector<futures::Promise<T>>;
  ContainerType promises;
};

struct CompactionManager : ICompactionManager,
                           std::enable_shared_from_this<CompactionManager> {
  explicit CompactionManager(
      IStorageManager& storage,
      std::shared_ptr<ReplicatedLogGlobalSettings const> options,
      LoggerContext const& loggerContext);

  CompactionManager(CompactionManager const&) = delete;
  CompactionManager(CompactionManager&&) noexcept = delete;
  auto operator=(CompactionManager const&) -> CompactionManager& = delete;
  auto operator=(CompactionManager&&) noexcept -> CompactionManager& = delete;

  void updateReleaseIndex(LogIndex index) noexcept override;
  void updateLowestIndexToKeep(LogIndex index) noexcept override;
  auto compact() noexcept -> futures::Future<CompactResult> override;

  struct Indexes {
    LogIndex releaseIndex, lowestIndexToKeep;
  };

  [[nodiscard]] auto getIndexes() const noexcept -> Indexes;

  auto getCompactionStatus() const noexcept -> CompactionStatus override;

  [[nodiscard]] static auto calculateCompactionIndex(LogIndex releaseIndex,
                                                     LogIndex lowestIndexToKeep,
                                                     LogRange bounds,
                                                     std::size_t threshold)
      -> std::tuple<LogIndex, CompactionStopReason>;

 private:
  struct GuardedData {
    explicit GuardedData(IStorageManager& storage);

    [[nodiscard]] auto isCompactionInProgress() const noexcept -> bool;

    ResolveAggregator<CompactResult> compactAggregator;

    bool _compactionInProgress{false};
    bool _fullCompactionNextRound{false};
    LogIndex releaseIndex;
    LogIndex lowestIndexToKeep;
    CompactionStatus status;
    IStorageManager& storage;
  };
  Guarded<GuardedData> guarded;
  LoggerContext const loggerContext;

  std::shared_ptr<ReplicatedLogGlobalSettings const> const options;

  void triggerAsyncCompaction(Guarded<GuardedData>::mutex_guard_type guard,
                              bool ignoreThreshold);
};
}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
