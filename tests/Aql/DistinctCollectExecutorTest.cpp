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

#include "AqlExecutorTestCase.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using DistinctCollectTestHelper = ExecutorTestHelper<1, 1>;
using DistinctCollectSplitType = DistinctCollectTestHelper::SplitType;

class DistinctCollectExecutorTest
    : public AqlExecutorTestCaseWithParam<std::tuple<DistinctCollectSplitType>> {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;

  std::unordered_set<RegisterId> readableInputRegisters = {0};
  std::unordered_set<RegisterId> writeableOutputRegisters = {1};

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  DistinctCollectExecutorInfos infos;

  DistinctCollectExecutorTest()
      : trx(fakedQuery->trx()),
        infos(1 /*nrIn*/, 2 /*nrOut*/, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::make_pair<RegisterId, RegisterId>(1, 0), trx) {}
};

TEST_P(DistinctCollectExecutorTest, split_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(infos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{2, AqlCall::Infinity{}, 2, true})
      .expectOutputValueList(3, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(DistinctCollectExecutorTest, split_3) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(infos))
      .setInputValueList(1, 2, 1, 2, 5, 4, 3, 3, 1, 2)
      .setInputSplitType(split)
      .setCall(AqlCall{2, AqlCall::Infinity{}, 2, true})
      .expectOutputValueList(5, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(DistinctCollectExecutorTest, split_2) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(infos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, 2, true})
      .expectOutputValueList(1, 2)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

template <size_t... vs>
const DistinctCollectSplitType splitIntoBlocks =
    DistinctCollectSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const DistinctCollectSplitType splitStep = DistinctCollectSplitType{step};

INSTANTIATE_TEST_CASE_P(DistinctCollectExecutor, DistinctCollectExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<2>, splitStep<1>));
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
