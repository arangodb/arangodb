////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/ExecutionState.h"
#include "Aql/types.h"
#include "Basics/ResourceUsage.h"
#include "Logger/LogMacros.h"
#include "Aql/Executor/GroupedSortExecutorBackend.h"

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class NoStats;
class OutputAqlItemRow;
template<BlockPassthrough>
class SingleRowFetcher;
struct SortRegister;

struct GroupedSortExecutorInfos {
  GroupedSortExecutorInfos(std::vector<SortRegister> sortRegisters,
                           std::vector<RegisterId> groupedRegisters,
                           bool stable, velocypack::Options const* vpackOptions,
                           ResourceMonitor& resourceMonitor);
  GroupedSortExecutorInfos() = delete;
  GroupedSortExecutorInfos(GroupedSortExecutorInfos&&) = default;
  GroupedSortExecutorInfos(GroupedSortExecutorInfos const&) = delete;
  ~GroupedSortExecutorInfos() = default;

  [[nodiscard]] std::vector<SortRegister> const& sortRegisters()
      const noexcept {
    return _sortRegisters;
  }
  [[nodiscard]] std::vector<RegisterId> const& groupedRegisters()
      const noexcept {
    return _groupedRegisters;
  }
  [[nodiscard]] bool stable() const { return _stable; }
  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept {
    return _vpackOptions;
  }
  [[nodiscard]] ResourceMonitor& getResourceMonitor() const {
    return _resourceMonitor;
  }

  std::vector<SortRegister> _sortRegisters;
  std::vector<RegisterId> _groupedRegisters;
  bool _stable = false;
  velocypack::Options const* _vpackOptions;
  ResourceMonitor& _resourceMonitor;
};

/**
 * @brief Implementation of Sort Node
 */
class GroupedSortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = GroupedSortExecutorInfos;
  using Stats = NoStats;

  GroupedSortExecutor(Fetcher&, Infos& infos);
  ~GroupedSortExecutor() = default;

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be sent to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be sent to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 private:
  group_sort::StorageBackend _storageBackend;
};

}  // namespace arangodb::aql
