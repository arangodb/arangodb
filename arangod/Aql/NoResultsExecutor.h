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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_NORESULTS_EXECUTOR_H
#define ARANGOD_AQL_NORESULTS_EXECUTOR_H

#include "Aql/EmptyExecutorInfos.h"
#include "Aql/ExecutionState.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

template <BlockPassthrough>
class SingleRowFetcher;
class RegisterInfos;
class NoStats;
struct AqlCall;
class OutputAqlItemRow;
class AqlItemBlockInputRange;

class NoResultsExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EmptyExecutorInfos;
  using Stats = NoStats;
  NoResultsExecutor(Fetcher&, Infos&);
  ~NoResultsExecutor();

  /**
   * @brief DO NOT PRODUCE ROWS
   *
   * @return DONE, NoStats, HardLimit = 0 Call
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output) const
      noexcept -> std::tuple<ExecutorState, Stats, AqlCall>;

  /**
   * @brief DO NOT SKIP ROWS
   *
   ** @return DONE, NoStats, 0, HardLimit = 0 Call
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call) const
      noexcept -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;
};
}  // namespace aql
}  // namespace arangodb

#endif
