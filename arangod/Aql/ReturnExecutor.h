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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_RETURN_EXECUTOR_H
#define ARANGOD_AQL_RETURN_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

template <bool>
class SingleRowFetcher;
class ExecutorInfos;
class NoStats;

class ReturnExecutorInfos : public ExecutorInfos {
 public:
  ReturnExecutorInfos(RegisterId inputRegister, RegisterId nrInputRegisters,
                      RegisterId nrOutputRegisters, bool doCount);

  ReturnExecutorInfos() = delete;
  ReturnExecutorInfos(ReturnExecutorInfos&&) = default;
  ReturnExecutorInfos(ReturnExecutorInfos const&) = delete;
  ~ReturnExecutorInfos() = default;

  RegisterId getInputRegisterId() const { return _inputRegisterId; }

  RegisterId getOutputRegisterId() const { return 0; }

  bool doCount() const { return _doCount; }

 private:
  /// @brief the variable produced by Return
  RegisterId _inputRegisterId;
  bool _doCount;
};

/**
 * @brief Implementation of Return Node
 *
 *
 * The return executor projects some column, given by _infos.getInputRegisterId(),
 * to the first and only column in the output. This is used for return nodes
 * in subqueries.
 * Return nodes on the top level use the IdExecutor instead.
 */
class ReturnExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    // The return executor is now only used for projecting some register to
    // register 0. So it does not pass through, but copy one column into a new
    // block with only this column.
    static const bool allowsBlockPassthrough = false;
    /* This could be set to true after some investigation/fixes */
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = ReturnExecutorInfos;
  using Stats = CountStats;

  ReturnExecutor(Fetcher& fetcher, ReturnExecutorInfos&);
  ~ReturnExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  inline std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output) {
    ExecutionState state;
    ReturnExecutor::Stats stats;
    InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    std::tie(state, inputRow) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!inputRow);
      return {state, stats};
    }

    if (!inputRow) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }

    AqlValue val = inputRow.stealValue(_infos.getInputRegisterId());
    AqlValueGuard guard(val, true);
    TRI_IF_FAILURE("ReturnBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    output.moveValueInto(_infos.getOutputRegisterId(), inputRow, guard);

    if (_infos.doCount()) {
      stats.incrCounted();
    }
    return {state, stats};
  }

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number of rows not supported");
  }

 private:
  ReturnExecutorInfos& _infos;
  Fetcher& _fetcher;
};
}  // namespace aql
}  // namespace arangodb

#endif
