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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "NoResultsExecutor.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"

namespace arangodb::aql {

NoResultsExecutor::NoResultsExecutor(Fetcher&, Infos&) {}
NoResultsExecutor::~NoResultsExecutor() = default;

auto NoResultsExecutor::produceRows(AqlItemBlockInputRange& input,
                                    OutputAqlItemRow& output) const noexcept
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return {input.upstreamState(), NoStats{},
          AqlCall{0, false, 0, AqlCall::LimitType::HARD}};
}

auto NoResultsExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                      AqlCall& call) const noexcept
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  return {inputRange.upstreamState(), NoStats{}, 0,
          AqlCall{0, false, 0, AqlCall::LimitType::HARD}};
};

[[nodiscard]] auto NoResultsExecutor::expectedNumberOfRows(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  // Well nevermind the input, but we will always return 0 rows here.
  return 0;
}

template class ExecutionBlockImpl<NoResultsExecutor>;

}  // namespace arangodb::aql
