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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_set>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class NoStats;
class RegisterInfos;
class OutputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;
struct AqlCall;
class AqlItemBlockInputRange;

class CountCollectExecutorInfos {
 public:
  explicit CountCollectExecutorInfos(RegisterId collectRegister);

  CountCollectExecutorInfos() = delete;
  CountCollectExecutorInfos(CountCollectExecutorInfos&&) = default;
  CountCollectExecutorInfos(CountCollectExecutorInfos const&) = delete;
  ~CountCollectExecutorInfos() = default;

 public:
  RegisterId getOutputRegisterId() const;

 private:
  RegisterId _collectRegister;
};

/**
 * @brief Implementation of Count Collect Executor
 */

class CountCollectExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CountCollectExecutorInfos;
  using Stats = NoStats;

  CountCollectExecutor() = delete;
  CountCollectExecutor(CountCollectExecutor&&) = default;
  CountCollectExecutor(CountCollectExecutor const&) = delete;
  CountCollectExecutor(Fetcher&, Infos&);
  ~CountCollectExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  Infos const& infos() const noexcept;

 private:
  Infos const& _infos;
};

}  // namespace aql
}  // namespace arangodb

#endif
