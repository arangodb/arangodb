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

#include "fakeit.hpp"

#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SortExecutor.h"
#include "Aql/ConstrainedSortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "search/sort.hpp"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class SortExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  velocypack::Options const* vpackOptions{&velocypack::Options::Defaults};

  Variable sortVar;
  SortElement sl;
  SortRegister sortReg;
  std::vector<SortRegister> sortRegisters;

  SortExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        sortVar("mySortVar", 0),
        sl(&sortVar, true),
        sortReg(0, sl) {
    sortRegisters.emplace_back(std::move(sortReg));
  }
};

TEST_F(SortExecutorTest, no_rows_upstream_producer_doesnt_wait) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, vpackOptions, false);
  VPackBuilder input;
  AllRowsFetcherHelper fetcher(input.steal(), false);
  SortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(SortExecutorTest, no_rows_upstream_producer_waits) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, vpackOptions, false);
  VPackBuilder input;
  AllRowsFetcherHelper fetcher(input.steal(), true);
  SortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}


TEST_F(SortExecutorTest, rows_upstream_we_are_waiting_for_list_of_numbers) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, vpackOptions, false);
  std::shared_ptr<VPackBuilder> input =
      VPackParser::fromJson("[[5],[3],[1],[2],[4]]");
  AllRowsFetcherHelper fetcher(input->steal(), true);
  SortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  // Wait, 5, Wait, 3, Wait, 1, Wait, 2, Wait, 4, HASMORE
  for (size_t i = 0; i < 5; ++i) {
    std::tie(state, stats) = testee.produceRows(result);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(result.produced());
  }

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  block = result.stealBlock();
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_EQ(number, 1);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(number, 2);

  v = block->getValue(2, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(number, 3);

  v = block->getValue(3, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(number, 4);

  v = block->getValue(4, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(number, 5);
}

}  // namespace arangodb::tests::aql
