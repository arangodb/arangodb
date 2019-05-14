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

#include "RowFetcherHelper.h"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class EnumerateListExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  EnumerateListExecutorTest() : itemBlockManager(&monitor) {}
};

TEST_F(EnumerateListExecutorTest, there_are_no_rows_upstream_the_producer_does_not_wait) {
  EnumerateListExecutorInfos infos(0, 1, 1, 2, {}, {0});
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  VPackBuilder input;

  SingleRowFetcherHelper<false> fetcher(input.steal(), false);
  EnumerateListExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}
TEST_F(EnumerateListExecutorTest, there_are_no_rows_upstream_the_producer_waits) {
  EnumerateListExecutorInfos infos(0, 1, 1, 2, {}, {0});
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  VPackBuilder input;

  SingleRowFetcherHelper<false> fetcher(input.steal(), true);
  EnumerateListExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(EnumerateListExecutorTest, there_is_one_row_in_the_upstream_the_producer_waits) {
  EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
  auto input = VPackParser::fromJson("[ [1, 2, 3, [true, true, true]] ]");

  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  EnumerateListExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  /*
   * Here we are not waiting after every row produce, because the fetcher
   * does not need to refetch a new row to produce the next one.
   * 1. produce => WAIT                 RES1  - due true flag in
   * SingleRowFetcherHelper
   * 2. produce => HASMORE              RES1 - return a row
   * 3. produce => HASMORE              RES2 - return a row
   * 4. produce => HASMORE              RES3 - return a row
   * 5. produce => DONE                 RES4 - DONE - do not return a row
   */

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  block = result.stealBlock();
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_TRUE(number == 1);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 1);

  v = block->getValue(1, 1);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 2);

  v = block->getValue(1, 2);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 3);

  bool mustDestroy = false;
  v = block->getValue(1, 3);
  ASSERT_TRUE(v.isArray());
  ASSERT_TRUE(v.at(0, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(1, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(2, mustDestroy, false).toBoolean() == true);

  v = block->getValue(1, 4);
  ASSERT_TRUE(v.isBoolean());
  ASSERT_TRUE(v.toBoolean() == true);
}

TEST_F(EnumerateListExecutorTest, there_is_one_empty_array_row_in_the_upstream_the_producer_waits) {
  EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
  auto input = VPackParser::fromJson("[ [1, 2, 3, [] ] ]");

  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  EnumerateListExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  block = result.stealBlock();
  ASSERT_TRUE(block == nullptr);
}

TEST_F(EnumerateListExecutorTest, there_are_rows_in_the_upstream_the_producer_waits) {
  EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
  auto input = VPackParser::fromJson(
      "[ [1, 2, 3, [true, true, true]], [1, 2, 3, [true, true, true]] ]");

  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  EnumerateListExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  // like the test above, except now two rows of input
  // are available

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  block = result.stealBlock();
  bool mustDestroy = false;

  // first row
  AqlValue v = block->getValue(0, 0);
  ASSERT_TRUE(v.isNumber());
  int64_t number = v.toInt64();
  ASSERT_TRUE(number == 1);

  v = block->getValue(1, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 1);

  v = block->getValue(1, 1);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 2);

  v = block->getValue(1, 2);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 3);

  v = block->getValue(1, 3);
  ASSERT_TRUE(v.isArray());
  ASSERT_TRUE(v.at(0, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(1, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(2, mustDestroy, false).toBoolean() == true);

  v = block->getValue(1, 4);
  ASSERT_TRUE(v.isBoolean());
  ASSERT_TRUE(v.toBoolean() == true);

  // second row
  v = block->getValue(2, 0);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 1);

  v = block->getValue(2, 1);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 2);

  v = block->getValue(2, 2);
  ASSERT_TRUE(v.isNumber());
  number = v.toInt64();
  ASSERT_TRUE(number == 3);

  v = block->getValue(2, 3);
  ASSERT_TRUE(v.isArray());
  ASSERT_TRUE(v.at(0, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(1, mustDestroy, false).toBoolean() == true);
  ASSERT_TRUE(v.at(2, mustDestroy, false).toBoolean() == true);

  v = block->getValue(2, 4);
  ASSERT_TRUE(v.isBoolean());
  ASSERT_TRUE(v.toBoolean() == true);
}

}  // namespace tests
}  // namespace arangodb
}  // namespace arangodb
