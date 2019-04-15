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

#ifndef ARANGOD_AQL_DISTINCT_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_DISTINCT_COLLECT_EXECUTOR_H

#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/LimitStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
template <bool>
class SingleRowFetcher;

class DistinctCollectExecutorInfos : public ExecutorInfos {
 public:
  DistinctCollectExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                               std::unordered_set<RegisterId> registersToClear,
                               std::unordered_set<RegisterId> registersToKeep,
                               std::unordered_set<RegisterId>&& readableInputRegisters,
                               std::unordered_set<RegisterId>&& writeableInputRegisters,
                               std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
                               transaction::Methods* trxPtr);

  DistinctCollectExecutorInfos() = delete;
  DistinctCollectExecutorInfos(DistinctCollectExecutorInfos&&) = default;
  DistinctCollectExecutorInfos(DistinctCollectExecutorInfos const&) = delete;
  ~DistinctCollectExecutorInfos() = default;

 public:
  std::vector<std::pair<RegisterId, RegisterId>> getGroupRegisters() const {
    return _groupRegisters;
  }
  transaction::Methods* getTransaction() const { return _trxPtr; }

 private:
  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief the transaction for this query
  transaction::Methods* _trxPtr;
};

/**
 * @brief Implementation of Distinct Collect Executor
 */

class DistinctCollectExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = DistinctCollectExecutorInfos;
  using Stats = NoStats;

  DistinctCollectExecutor() = delete;
  DistinctCollectExecutor(DistinctCollectExecutor&&) = default;
  DistinctCollectExecutor(DistinctCollectExecutor const&) = delete;
  DistinctCollectExecutor(Fetcher& fetcher, Infos&);
  ~DistinctCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  Infos const& infos() const noexcept { return _infos; };

 private:
  Infos const& _infos;
  Fetcher& _fetcher;
  std::unordered_set<std::vector<AqlValue>, AqlValueGroupHash, AqlValueGroupEqual> _seen;
};

}  // namespace aql
}  // namespace arangodb

#endif
