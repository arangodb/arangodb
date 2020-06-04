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

#ifndef ARANGOD_AQL_SUBQUERY_START_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_START_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

#include <utility>

namespace arangodb {
namespace aql {

template <BlockPassthrough allowsPassThrough>
class SingleRowFetcher;
class NoStats;
class RegisterInfos;
class OutputAqlItemRow;

class SubqueryStartExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = RegisterInfos;
  using Stats = NoStats;
  SubqueryStartExecutor(Fetcher&, Infos& infos);
  ~SubqueryStartExecutor() = default;

  // produceRows for SubqueryStart reads a data row from its input and produces
  // a copy of that row and a shadow row. This requires some amount of internal
  // state as it can happen that after producing the copied data row the output
  // is full, and hence we need to return ExecutorState::HASMORE to be able to
  // produce the shadow row
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  // skipRowsRange just skips input rows and reports how many rows it skipped
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  // Produce a shadow row *if* we have either skipped or output a datarow
  // previously
  auto produceShadowRow(AqlItemBlockInputRange& input, OutputAqlItemRow& output) -> bool;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  // Upstream state, used to determine if we are done with all subqueries
  ExecutorState _upstreamState{ExecutorState::HASMORE};

  // Cache for the input row we are currently working on
  InputAqlItemRow _inputRow{CreateInvalidInputRowHint{}};
};
}  // namespace aql
}  // namespace arangodb
#endif
