////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Basics/Result.h"
#include "Transaction/Methods.h"

// TODO: the entire executor is only needed to execute non-spliced subqueries sent
// by a 3.7 coordinator. It can be removed in 3.9
namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
class Builder;
}

namespace aql {
class ExecutionBlock;
class NoStats;
class OutputAqlItemRow;
class QueryContext;
template <BlockPassthrough>
class SingleRowFetcher;

class SubqueryExecutorInfos {
 public:
  SubqueryExecutorInfos(ExecutionBlock& subQuery, QueryContext& query,
                        RegisterId outReg, bool subqueryIsConst);

  SubqueryExecutorInfos() = delete;
  SubqueryExecutorInfos(SubqueryExecutorInfos&&) = default;
  SubqueryExecutorInfos(SubqueryExecutorInfos const&) = delete;
  ~SubqueryExecutorInfos() = default;

  inline ExecutionBlock& getSubquery() const { return _subQuery; }
  aql::QueryContext& query() noexcept;
  inline bool returnsData() const { return _returnsData; }
  inline RegisterId outputRegister() const { return _outReg; }
  inline bool isConst() const { return _isConst; }

 private:
  ExecutionBlock& _subQuery;
  QueryContext& _query;
  RegisterId const _outReg;
  bool const _returnsData;
  bool const _isConst;
};

template <bool isModificationSubquery>
class SubqueryExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        isModificationSubquery ? BlockPassthrough::Disable : BlockPassthrough::Enable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SubqueryExecutorInfos;
  using Stats = NoStats;

  SubqueryExecutor(Fetcher& fetcher, SubqueryExecutorInfos& infos);
  ~SubqueryExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutionState, Stats, AqlCall>;

  // skipRowsRange <=> isModificationSubquery

  template <bool E = isModificationSubquery, std::enable_if_t<E, int> = 0>
  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutionState, Stats, size_t, AqlCall>;

 private:
  /**
   * actually write the subquery output to the line
   * Handles reset of local state variables
   */
  void writeOutput(OutputAqlItemRow& output);

  /**
   * @brief Translate _state => to to execution allowing waiting.
   *
   */
  auto translatedReturnType() const noexcept -> ExecutionState;

  /**
   * @brief Initiliaze the subquery with next input row
   *        Throws if there was an error during initialize cursor
   *
   *
   * @param input Container for more data
   * @return std::tuple<ExecutionState, bool> Result state (WAITING or
   * translatedReturnType())
   * bool flag if we have initialized the query, if not, we require more data.
   */
  auto initializeSubquery(AqlItemBlockInputRange& input)
      -> std::tuple<ExecutionState, bool>;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  Fetcher& _fetcher;
  SubqueryExecutorInfos& _infos;
  
  transaction::Methods _trx;
    
  // Upstream state, used to determine if we are done with all subqueries
  ExecutorState _state;

  // Flag if the current subquery is initialized and worked on
  bool _subqueryInitialized;

  // Flag if we have correctly triggered shutdown
  bool _shutdownDone;

  // Result of subquery Shutdown
  Result _shutdownResult;

  // The root node of the subqery
  ExecutionBlock& _subquery;

  // Place where the current subquery can store intermediate results.
  arangodb::velocypack::Buffer<uint8_t> _subqueryResults;

  arangodb::velocypack::Builder _subqueryResultsBuilder;

  // Cache for the input row we are currently working on
  InputAqlItemRow _input;

  size_t _skipped = 0;
};
}  // namespace aql
}  // namespace arangodb

#endif
