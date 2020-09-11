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

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "ExecutorTestHelper.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
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
  // arangodb::transaction::Methods* trx;

  RegIdSet readableInputRegisters = RegIdSet{0};
  RegIdSet writeableOutputRegisters = RegIdSet{1};

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  RegisterInfos registerInfos;
  DistinctCollectExecutorInfos executorInfos;

  DistinctCollectExecutorTest()
      : registerInfos(std::move(readableInputRegisters), std::move(writeableOutputRegisters),
                      1, 2, RegIdFlatSet{}, RegIdFlatSetStack{{}}),
        executorInfos(std::make_pair<RegisterId, RegisterId>(1, 0), &VPackOptions::Defaults) {}
};

TEST_P(DistinctCollectExecutorTest, split_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{2u, AqlCall::Infinity{}, 2u, true})
      .expectOutputValueList(3, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(DistinctCollectExecutorTest, split_3) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 2, 1, 2, 5, 4, 3, 3, 1, 2)
      .setInputSplitType(split)
      .setCall(AqlCall{2u, AqlCall::Infinity{}, 2u, true})
      .expectOutputValueList(5, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(DistinctCollectExecutorTest, split_2) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<DistinctCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, 2u, true})
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
