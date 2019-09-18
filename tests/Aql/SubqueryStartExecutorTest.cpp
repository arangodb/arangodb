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

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryStartExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
ExecutorInfos MakeBaseInfos(RegisterId numRegs) {
  auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{});
  std::unordered_set<RegisterId> toKeep;
  for (RegisterId r = 0; r < numRegs; ++r) {
    toKeep.emplace(r);
  }
  return ExecutorInfos(emptyRegisterList, emptyRegisterList, numRegs, numRegs, {}, toKeep);
}
}  // namespace

class SubqueryStartExecutorTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
};

TEST_F(SubqueryStartExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryStartExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_FALSE(SubqueryStartExecutor::Properties::allowsBlockPassthrough)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryStartExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block is restricted by input, it will atMost produce 2 times the "
         "input. (Might be less if input contains shadowRows";
}

TEST_F(SubqueryStartExecutorTest, empty_input_does_not_add_shadow_rows) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), false);
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  OutputAqlItemRow ouput{std::move(block), infos.getOutputRegisters(),
                         infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(ouput);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!ouput.produced());
  ASSERT_EQ(ouput.numRowsLeft(), 1000);
}