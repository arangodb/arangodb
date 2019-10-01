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

namespace arangodb {
namespace tests {
namespace aql {

int compareAqlValues(irs::sort::prepared const*, arangodb::transaction::Methods* trx,
                     arangodb::aql::AqlValue const& lhs,
                     arangodb::aql::AqlValue const& rhs) {
  return arangodb::aql::AqlValue::Compare(trx, lhs, rhs, true);
}

class SortExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  // Mock of the Transaction
  // Enough for this test, will only be passed through and accessed
  // on documents alone.
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx;

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& ctxt;

  Variable sortVar;
  SortElement sl;
  SortRegister sortReg;
  std::vector<SortRegister> sortRegisters;

  SortExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        trx(mockTrx.get()),
        ctxt(mockContext.get()),
        sortVar("mySortVar", 0),
        sl(&sortVar, true),
        sortReg(0, sl) {
    fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&ctxt);
    fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&arangodb::velocypack::Options::Defaults);
    sortRegisters.emplace_back(std::move(sortReg));
  }
};

TEST_F(SortExecutorTest, no_rows_upstream_producer_doesnt_wait) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, &trx, false);
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
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!result.produced());
}

TEST_F(SortExecutorTest, no_rows_upstream_producer_waits) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, &trx, false);
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
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!result.produced());
}


TEST_F(SortExecutorTest, rows_upstream_we_are_waiting_for_list_of_numbers) {
  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, &trx, false);
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
    ASSERT_EQ(ExecutionState::WAITING, state);
    ASSERT_TRUE(!result.produced());
  }

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(result.produced());

  block = result.stealBlock();
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_EQ(1, number);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(2, number);

  v = block->getValue(2, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(3, number);

  v = block->getValue(3, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(4, number);

  v = block->getValue(4, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(5, number);
}

TEST_F(SortExecutorTest, constrained_no_rows_upstream_producer_doesnt_wait) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), false);
  ConstrainedSortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!result.produced());
}

TEST_F(SortExecutorTest, constrained_no_rows_upstream_producer_doesnt_wait_skiprows) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), false);
  ConstrainedSortExecutor testee(fetcher, infos);

  size_t skipped;
  NoStats stats{};
  std::tie(state, stats, skipped) = testee.skipRows(1000);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_EQ(0, skipped);
}

TEST_F(SortExecutorTest, constrained_no_rows_upstream_producer_waits) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), true);
  ConstrainedSortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!result.produced());
}

TEST_F(SortExecutorTest, constrained_no_rows_upstream_producer_waits_skiprows) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), true);
  ConstrainedSortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};
  size_t skipped;
  std::tie(state, stats, skipped) = testee.skipRows(1000);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_EQ(0, skipped);

  std::tie(state, stats, skipped) = testee.skipRows(1000);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_EQ(0, skipped);
}

TEST_F(SortExecutorTest, constrained_rows_upstream_we_are_waiting_for_list_of_numbers) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  std::shared_ptr<VPackBuilder> input =
      VPackParser::fromJson("[[5],[3],[1],[2],[4]]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), true);
  ConstrainedSortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  // Wait, 5, Wait, 3, Wait, 1, Wait, 2, Wait, 4, HASMORE
  for (size_t i = 0; i < 5; ++i) {
    std::tie(state, stats) = testee.produceRows(result);
    ASSERT_EQ(ExecutionState::WAITING, state);
    ASSERT_TRUE(!result.produced());
  }

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());

  std::tie(state, stats, std::ignore) = testee.skipRows(1);
  ASSERT_EQ(ExecutionState::DONE, state);

  block = result.stealBlock();
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_EQ(1, number);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(2, number);

  v = block->getValue(2, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(3, number);

  v = block->getValue(3, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(4, number);

  v = block->getValue(4, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(5, number);
}

TEST_F(SortExecutorTest, constrained_rows_upstream_we_are_waiting_for_list_of_numbers_skiprows) {
  SortExecutorInfos infos(std::move(sortRegisters),
        100, itemBlockManager, 1, 1,
        {}, {0}, &trx, false);
  std::shared_ptr<VPackBuilder> input =
      VPackParser::fromJson("[[5],[3],[1],[2],[4]]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), true);
  ConstrainedSortExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};
  size_t skipped;
  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  // Wait, 5, Wait, 3, Wait, 1, Wait, 2, Wait, 4, HASMORE
  for (size_t i = 0; i < 5; ++i) {
    std::tie(state, stats, skipped) = testee.skipRows(10);
    ASSERT_EQ(ExecutionState::WAITING, state);
    ASSERT_EQ(0, skipped);
  }

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats, skipped) = testee.skipRows(1);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_EQ(skipped, 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats, skipped) = testee.skipRows(2);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_EQ(2, skipped);

  block = result.stealBlock();
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_EQ(1, number);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_EQ(3, number);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
