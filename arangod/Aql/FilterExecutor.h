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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_FILTER_EXECUTOR_H
#define ARANGOD_AQL_FILTER_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class OutputAqlItemRow;
class RegisterInfos;
class FilterStats;
template <BlockPassthrough>
class SingleRowFetcher;

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
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
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
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  Infos& _infos;
};

}  // namespace arangodb::aql

#endif
