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

#ifndef ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H

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

class CountCollectExecutorInfos : public ExecutorInfos {
 public:
  CountCollectExecutorInfos(RegisterId collectRegister, RegisterId nrInputRegisters,
                            RegisterId nrOutputRegisters,
                            std::unordered_set<RegisterId> registersToClear,
                            std::unordered_set<RegisterId> registersToKeep);

  CountCollectExecutorInfos() = delete;
  CountCollectExecutorInfos(CountCollectExecutorInfos&&) = default;
  CountCollectExecutorInfos(CountCollectExecutorInfos const&) = delete;
  ~CountCollectExecutorInfos() = default;

 public:
  RegisterId getOutputRegisterId() const { return _collectRegister; }

 private:
  RegisterId _collectRegister;
};

/**
 * @brief Implementation of Count Collect Executor
 */

class CountCollectExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CountCollectExecutorInfos;
  using Stats = NoStats;

  CountCollectExecutor() = delete;
  CountCollectExecutor(CountCollectExecutor&&) = default;
  CountCollectExecutor(CountCollectExecutor const&) = delete;
  CountCollectExecutor(Fetcher& fetcher, Infos&);
  ~CountCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */

  inline std::pair<ExecutionState, NoStats> produceRows(OutputAqlItemRow& output) {
    TRI_IF_FAILURE("CountCollectExecutor::produceRows") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (_state == ExecutionState::DONE) {
      return {_state, NoStats{}};
    }

    while (_state != ExecutionState::DONE) {
      size_t skipped;
      std::tie(_state, skipped) = _fetcher.skipRows(ExecutionBlock::SkipAllSize());

      if (_state == ExecutionState::WAITING) {
        TRI_ASSERT(skipped == 0);
        return {_state, NoStats{}};
      }

      TRI_ASSERT(skipped != 0 || _state == ExecutionState::DONE);
      incrCountBy(skipped);
    }

    // In general, we do not have an input row. In fact, we never fetch one.
    output.setAllowSourceRowUninitialized();

    // We must produce exactly one output row.
    output.cloneValueInto(_infos.getOutputRegisterId(),
                          InputAqlItemRow{CreateInvalidInputRowHint{}},
                          AqlValue(AqlValueHintUInt(getCount())));

    return {_state, NoStats{}};
  }

  void incrCountBy(size_t incr) noexcept { _count += incr; };
  uint64_t getCount() noexcept { return _count; };

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const {
    if (_state == ExecutionState::DONE) {
      return {ExecutionState::DONE, 0};
    }
    return {ExecutionState::HASMORE, 1};
  }

 private:
  Infos const& infos() const noexcept { return _infos; };

 private:
  Infos const& _infos;
  Fetcher& _fetcher;
  ExecutionState _state;
  uint64_t _count;
};

}  // namespace aql
}  // namespace arangodb

#endif
