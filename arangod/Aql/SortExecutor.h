////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_EXECUTOR_H
#define ARANGOD_AQL_SORT_EXECUTOR_H

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"

#include <cstddef>
#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AllRowsFetcher;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
class AqlItemBlockManager;
struct SortRegister;

class SortExecutorInfos : public ExecutorInfos {
 public:
  SortExecutorInfos(std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager, RegisterId nrInputRegisters,
                    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
                    std::unordered_set<RegisterId> registersToKeep,
                    velocypack::Options const*, bool stable);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos&&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;

  [[nodiscard]] std::vector<SortRegister> const& sortRegisters() const noexcept;

  [[nodiscard]] bool stable() const;

  [[nodiscard]] size_t limit() const noexcept;

  [[nodiscard]] AqlItemBlockManager& itemBlockManager() noexcept;

 private:
  std::size_t _limit;
  AqlItemBlockManager& _manager;
  velocypack::Options const* _vpackOptions;
  std::vector<SortRegister> _sortRegisters;
  bool _stable;
};

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = AllRowsFetcher;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  SortExecutor(Fetcher& fetcher, Infos&);
  ~SortExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const;

 private:
  void doSorting();

 private:
  Infos& _infos;

  Fetcher& _fetcher;

  AqlItemMatrix const* _input;

  std::vector<AqlItemMatrix::RowIndex> _sortedIndexes;

  size_t _returnNext;
};
}  // namespace aql
}  // namespace arangodb

#endif
