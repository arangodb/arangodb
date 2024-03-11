////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Aql/ExecutionState.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/types.h"

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class OutputAqlItemRow;
class FilterStats;

class FilterExecutorInfos {
 public:
  explicit FilterExecutorInfos(RegisterId inputRegister);

  FilterExecutorInfos() = delete;
  FilterExecutorInfos(FilterExecutorInfos&&) = default;
  FilterExecutorInfos(FilterExecutorInfos const&) = delete;
  ~FilterExecutorInfos() = default;

  [[nodiscard]] RegisterId getInputRegister() const noexcept;

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;
};

/**
 * @brief Implementation of Filter Node
 */
class FilterExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = FilterExecutorInfos;
  using Stats = FilterStats;

  FilterExecutor() = delete;
  FilterExecutor(FilterExecutor&&) = default;
  FilterExecutor(FilterExecutor const&) = delete;
  FilterExecutor(Fetcher& fetcher, Infos&);
  ~FilterExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

  [[nodiscard]] auto expectedNumberOfRows(AqlItemBlockInputRange const& input,
                                          AqlCall const& call) const noexcept
      -> size_t;

 private:
  Infos& _infos;
};

}  // namespace arangodb::aql
