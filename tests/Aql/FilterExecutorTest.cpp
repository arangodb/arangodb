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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlItemBlockHelper.h"
#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using FilterExecutorTestHelper = ExecutorTestHelper<2, 2>;
using FilterExecutorSplitType = FilterExecutorTestHelper::SplitType;
using FilterExecutorInputParam = std::tuple<FilterExecutorSplitType>;

class FilterExecutorTest : public AqlExecutorTestCaseWithParam<FilterExecutorInputParam> {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>>& registersToKeep;
  FilterExecutorInfos infos;

  FilterExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToKeep(outputRegisters),
        infos(0, 1, 1, {}, {}) {}

  auto getSplit() -> FilterExecutorSplitType {
    auto [split] = GetParam();
    return split;
  }

  auto buildInfos() -> FilterExecutorInfos {
    return FilterExecutorInfos{0, 2, 2, {}, {0, 1}};
  }
};

template <size_t... vs>
const FilterExecutorSplitType splitIntoBlocks =
    FilterExecutorSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const FilterExecutorSplitType splitStep = FilterExecutorSplitType{step};

INSTANTIATE_TEST_CASE_P(FilterExecutor, FilterExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<1>, splitStep<2>));

TEST_P(FilterExecutorTest, empty_input) {
  auto infos = buildInfos();
  AqlCall call{};
  ExecutionStats{};
  ExecutorTestHelper(*fakedQuery)
      .setExecBlock<FilterExecutor>(std::move(infos))
      .setInputValue({})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(FilterExecutorTest, values) {
  auto infos = buildInfos();
  AqlCall call{};
  ExecutionStats{};
  ExecutorTestHelper<2, 2>(*fakedQuery)
      .setExecBlock<FilterExecutor>(std::move(infos))
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 0}, RowBuilder<2>{0, 1},
                                      RowBuilder<2>{0, 2}, RowBuilder<2>{0, 3},
                                      RowBuilder<2>{0, 4}, RowBuilder<2>{0, 5},
                                      RowBuilder<2>{0, 6}, RowBuilder<2>{0, 7}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{1, 0}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(FilterExecutorTest, odd_values) {
  auto infos = buildInfos();
  AqlCall call{};
  ExecutionStats{};
  ExecutorTestHelper<2, 2>(*fakedQuery)
      .setExecBlock<FilterExecutor>(std::move(infos))
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 0}, RowBuilder<2>{0, 1},
                                      RowBuilder<2>{1, 2}, RowBuilder<2>{0, 3},
                                      RowBuilder<2>{1, 4}, RowBuilder<2>{0, 5},
                                      RowBuilder<2>{1, 6}, RowBuilder<2>{0, 7}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{1, 0}, RowBuilder<2>{1, 2},
                                             RowBuilder<2>{1, 4}, RowBuilder<2>{1, 6}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(FilterExecutorTest, skip_and_odd_values) {
  auto infos = buildInfos();
  AqlCall call{3};
  ExecutionStats{};
  ExecutorTestHelper<2, 2>(*fakedQuery)
      .setExecBlock<FilterExecutor>(std::move(infos))
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 0}, RowBuilder<2>{0, 1},
                                      RowBuilder<2>{1, 2}, RowBuilder<2>{0, 3},
                                      RowBuilder<2>{1, 4}, RowBuilder<2>{0, 5},
                                      RowBuilder<2>{1, 6}, RowBuilder<2>{0, 7}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{1, 6}})
      .allowAnyOutputOrder(false)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(FilterExecutorTest, hard_limit) {
  auto infos = buildInfos();
  AqlCall call{};
  call.hardLimit = 0;
  call.fullCount = true;
  ExecutionStats{};
  ExecutorTestHelper<2, 2>(*fakedQuery)
      .setExecBlock<FilterExecutor>(std::move(infos))
      .setInputValue(MatrixBuilder<2>{})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}  // namespace aql

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
