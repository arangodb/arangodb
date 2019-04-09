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
/// @author Daniel Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CONSTRAINED_SORT_EXECUTOR_H
#define ARANGOD_AQL_CONSTRAINED_SORT_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortNode.h"
#include "AqlValue.h"

#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

template <bool>
class SingleRowFetcher;

class AqlItemBlockShell;
class AqlItemMatrix;
class ConstrainedLessThan;
class ExecutorInfos;
class InputAqlItemRow;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

/**
 * @brief Implementation of Sort Node
 */
class ConstrainedSortExecutor {
 public:
  friend class Sorter;

  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  ConstrainedSortExecutor(Fetcher& fetcher, Infos&);
  ~ConstrainedSortExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  /**
   * @brief This Executor knows how many rows it will produce and most by itself
   *        It also knows that it could produce less if the upstream only has fewer rows.
   */
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  bool compareInput(uint32_t const& rosPos, InputAqlItemRow& row) const;
  arangodb::Result pushRow(InputAqlItemRow& row);

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  ExecutionState _state;
  size_t _returnNext;
  std::vector<uint32_t> _rows;
  size_t _rowsPushed;
  std::shared_ptr<arangodb::aql::AqlItemBlockShell> _heapBuffer;
  std::unique_ptr<ConstrainedLessThan> _cmpHeap;  // in pointer to avoid
  OutputAqlItemRow _heapOutputRow;
};
}  // namespace aql
}  // namespace arangodb

#endif
