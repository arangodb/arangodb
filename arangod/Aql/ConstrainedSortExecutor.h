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
#include "Aql/SortNode.h"
#include "Aql/SortExecutor.h"
#include "AqlValue.h"

#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

template <bool>
class SingleRowFetcher;
class AqlItemMatrix;
class ExecutorInfos;
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

 private:
  void doSorting();

 private:
  Infos& _infos;

  Fetcher& _fetcher;

  AqlItemMatrix const* _input;

  std::vector<size_t> _sortedIndexes;

  size_t _returnNext;

  std::unordered_map<AqlValue, AqlValue> _cache;
};
}  // namespace aql
}  // namespace arangodb

#endif
