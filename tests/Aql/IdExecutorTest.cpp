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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IdExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"

#include "tests/Aql/RowFetcherHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class IdExecutorTest : public ::testing::Test {
protected: 
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>> registersToKeep;  // this must be set correctly

  IdExecutorInfos infos;
  OutputAqlItemRow row;

  IdExecutorTest()
      : itemBlockManager(&monitor),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToKeep(make_shared_unordered_set({0})),
        infos(1 /*nrRegs*/, *registersToKeep /*toKeep*/, {} /*toClear*/),
        row(std::move(block), outputRegisters, registersToKeep, infos.registersToClear()) {}
};

TEST_F(IdExecutorTest, there_are_no_rows_upstream) {
  ConstFetcherHelper fetcher(itemBlockManager, nullptr);
  IdExecutor<ConstFetcher> testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!row.produced());
}

TEST_F(IdExecutorTest, there_are_rows_in_the_upstream) {
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  ConstFetcherHelper fetcher(itemBlockManager, input->buffer());
  IdExecutor<ConstFetcher> testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!row.produced());

  // verify result
  AqlValue value;
  auto block = row.stealBlock();
  for (std::size_t index = 0; index < 3; index++) {
    value = block->getValue(index, 0);
    ASSERT_TRUE(value.isBoolean());
    ASSERT_TRUE(value.toBoolean() == input->slice().at(index).at(0).getBool());
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
