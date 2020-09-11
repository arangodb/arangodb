////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CONSTRAINED_SORT_EXECUTOR_H
#define ARANGOD_AQL_CONSTRAINED_SORT_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace arangodb {
class Result;
namespace transaction {
class Methods;
}

namespace aql {

template <BlockPassthrough>
class SingleRowFetcher;

class AqlItemMatrix;
class ConstrainedLessThan;
class RegisterInfos;
class InputAqlItemRow;
class AqlItemBlockInputRange;
class NoStats;
class OutputAqlItemRow;
class SortExecutorInfos;
struct SortRegister;

/**
 * @brief Implementation of Sort Node
 */
class ConstrainedSortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  ConstrainedSortExecutor(Fetcher& fetcher, Infos&);
  ~ConstrainedSortExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  /**
   * @brief skip the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  /**
   * @brief This Executor knows how many rows it will produce and most by itself
   *        It also knows that it could produce less if the upstream only has fewer rows.
   */
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  bool compareInput(size_t const& rosPos, InputAqlItemRow const& row) const;
  arangodb::Result pushRow(InputAqlItemRow const& row);

  // We're done producing when we've emitted all rows from our heap.
  bool doneProducing() const noexcept;

  // We're done skipping when we've emitted all rows from our heap,
  // AND emitted (in this case, skipped) all rows that were dropped during the
  // sort as well. This is for fullCount queries only.
  bool doneSkipping() const noexcept;

  ExecutorState consumeInput(AqlItemBlockInputRange& inputRange);

 private:
  Infos& _infos;
  size_t _returnNext;
  std::vector<size_t> _rows;
  size_t _rowsPushed;
  size_t _rowsRead;
  size_t _skippedAfter;
  SharedAqlItemBlockPtr _heapBuffer;
  std::unique_ptr<ConstrainedLessThan> _cmpHeap;  // in pointer to avoid
  RegIdFlatSetStack _regsToKeep;
  RegIdSet  _outputRegister = {};
  OutputAqlItemRow _heapOutputRow;
};
}  // namespace aql
}  // namespace arangodb

#endif
