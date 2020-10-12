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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_RETURN_EXECUTOR_H
#define ARANGOD_AQL_RETURN_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class RegisterInfos;
class NoStats;

class ReturnExecutorInfos {
 public:
  ReturnExecutorInfos(RegisterId inputRegister, bool doCount);

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
    static constexpr bool preservesOrder = true;
    // The return executor is now only used for projecting some register to
    // register 0. So it does not pass through, but copy one column into a new
    // block with only this column.
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = ReturnExecutorInfos;
  using Stats = CountStats;

  ReturnExecutor(Fetcher& fetcher, ReturnExecutorInfos&);
  ~ReturnExecutor();

  /**
   * @brief skip the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  ReturnExecutorInfos& _infos;
};
}  // namespace aql
}  // namespace arangodb

#endif
