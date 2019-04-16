////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Stats.h"

namespace arangodb {
namespace aql {

class NoStats;
class OutputAqlItemRow;
template <bool>
class SingleRowFetcher;

class SubqueryExecutorInfos : public ExecutorInfos {
 public:
  SubqueryExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
                        std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
                        RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                        std::unordered_set<RegisterId> const& registersToClear,
                        std::unordered_set<RegisterId>&& registersToKeep,
                        ExecutionBlock& subQuery, RegisterId outReg, bool subqueryIsConst);

  SubqueryExecutorInfos() = delete;
  SubqueryExecutorInfos(SubqueryExecutorInfos&&);
  SubqueryExecutorInfos(SubqueryExecutorInfos const&) = delete;
  ~SubqueryExecutorInfos();

  inline ExecutionBlock& getSubquery() const { return _subQuery; }
  inline bool returnsData() const { return _returnsData; }
  inline RegisterId outputRegister() const { return _outReg; }
  inline bool isConst() const { return _isConst; }

 private:
  ExecutionBlock& _subQuery;
  RegisterId const _outReg;
  bool const _returnsData;
  bool const _isConst;
};

class SubqueryExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = true;
    static const bool inputSizeRestrictsOutputSize = false;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SubqueryExecutorInfos;
  using Stats = NoStats;

  SubqueryExecutor(Fetcher& fetcher, SubqueryExecutorInfos& infos);
  ~SubqueryExecutor();

  /**
   * @brief Shutdown will be called once for every query
   *
   * @return ExecutionState and no error.
   */
  std::pair<ExecutionState, Result> shutdown(int errorCode);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const {
    // Passthrough does not need to implement this!
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 private:
  /**
   * actually write the subquery output to the line
   * Handles reset of local state variables
   */
  void writeOutput(OutputAqlItemRow& output);

 private:
  Fetcher& _fetcher;
  SubqueryExecutorInfos& _infos;

  // Upstream state, used to determine if we are done with all subqueries
  ExecutionState _state;

  // Flag if the current subquery is initialized and worked on
  bool _subqueryInitialized;

  // Flag if we have correctly triggered shutdown
  bool _shutdownDone;

  // Result of subquery Shutdown
  Result _shutdownResult;

  // The root node of the subqery
  ExecutionBlock& _subquery;

  // Place where the current subquery can store intermediate results.
  std::unique_ptr<std::vector<SharedAqlItemBlockPtr>> _subqueryResults;

  // Cache for the input row we are currently working on
  InputAqlItemRow _input;
};
}  // namespace aql
}  // namespace arangodb

#endif
