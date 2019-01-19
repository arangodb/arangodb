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

#ifndef ARANGOD_AQL_LIMIT_EXECUTOR_H
#define ARANGOD_AQL_LIMIT_EXECUTOR_H

#include "Aql/ExecutionBlockImpl.h"
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
class SingleRowFetcher;

class LimitExecutorInfos : public ExecutorInfos {
 public:
  LimitExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                     std::unordered_set<RegisterId> registersToClear,
                     size_t offset, size_t limit, bool fullCount, size_t queryDepth);

  LimitExecutorInfos() = delete;
  LimitExecutorInfos(LimitExecutorInfos&&) = default;
  LimitExecutorInfos(LimitExecutorInfos const&) = delete;
  ~LimitExecutorInfos() = default;

  RegisterId getInputRegister() const noexcept { return _inputRegister; };
  size_t getLimit() { return _limit; };
  size_t getRemainingOffset() { return _remainingOffset; };
  void decrRemainingOffset() { _remainingOffset--; };
  size_t getQueryDepth() { return _queryDepth; };
  bool isFullCountEnabled() { return _fullCount; };

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;

  /// @brief the remaining offset
  size_t _remainingOffset;

  /// @brief the limit
  size_t _limit;

  /// @brief the depth of the query (e.g. subquery)
  size_t _queryDepth;

  /// @brief whether or not the node should fully count what it limits
  bool _fullCount;
};

/**
 * @brief Implementation of Limit Node
 */

class LimitExecutor {
 public:
  using Fetcher = SingleRowFetcher;
  using Infos = LimitExecutorInfos;
  using Stats = LimitStats;

  LimitExecutor() = delete;
  LimitExecutor(LimitExecutor&&) = default;
  LimitExecutor(LimitExecutor const&) = delete;
  LimitExecutor(Fetcher& fetcher, Infos&);
  ~LimitExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  ExecutionState handleSingleRow(OutputAqlItemRow& output, Stats& stats, bool skipOffset);

  Infos& _infos;
  Fetcher& _fetcher;
  uint64_t _counter ;
  bool _done;
  bool _doFullCount;
};

}  // namespace aql
}  // namespace arangodb

#endif
