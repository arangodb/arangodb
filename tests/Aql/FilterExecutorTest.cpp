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

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class FilterExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>>& registersToKeep;
  FilterExecutorInfos infos;

  FilterExecutorTest()
      : itemBlockManager(&monitor),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToKeep(outputRegisters),
        infos(0, 1, 1, {}, {}) {}
};

TEST_F(FilterExecutorTest, there_are_no_rows_upstream_the_producer_does_not_wait) {
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(FilterExecutorTest, there_are_no_rows_upstream_the_producer_waits) {
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);
}

TEST_F(FilterExecutorTest, there_are_rows_in_the_upstream_the_producer_does_not_wait) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(stats.getFiltered() == 0);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(stats.getFiltered() == 1);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(stats.getFiltered() == 2);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(stats.getFiltered() == 0);
  ASSERT_TRUE(!row.produced());
}

TEST_F(FilterExecutorTest, there_are_rows_in_the_upstream_the_producer_waits) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  /*
  1  produce => WAIT                 RES1
  2  produce => HASMORE, Row 1     RES1
  3  => WAIT                         RES2
  4  => WAIT                         RES2
  5   => HASMORE, Row 3            RES2
  6   => WAIT,                       RES3
  7   => WAIT,                       RES3
  8   => WAIT,                       RES3
  9   => DONE, Row 6               RES3
  */

  // 1
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  // 2
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_TRUE(stats.getFiltered() == 0);

  // 3
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  // 4
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  // We have one filter here
  ASSERT_TRUE(stats.getFiltered() == 1);

  // 5
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_TRUE(stats.getFiltered() == 0);

  // 6
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  // 7
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);

  // 7
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);

  // 8
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_TRUE(stats.getFiltered() == 0);
}

TEST_F(FilterExecutorTest,
       there_are_rows_in_the_upstream_and_the_last_one_has_to_be_filtered_the_producer_does_not_wait) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true], [false] ]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(stats.getFiltered() == 0);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(stats.getFiltered() == 1);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(stats.getFiltered() == 2);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(stats.getFiltered() == 1);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(stats.getFiltered() == 0);
  ASSERT_TRUE(!row.produced());
}

TEST_F(FilterExecutorTest,
       there_are_rows_in_the_upstream_and_the_last_one_has_to_be_filtered_the_producer_waits) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true], [false] ]");
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());

  /*
  produce => WAIT                  RES1
  produce => HASMORE, Row 1        RES1
  => WAIT                          RES2
  => WAIT                          RES2
   => HASMORE, Row 3               RES2
   => WAIT,                        RES3
   => WAIT,                        RES3
   => WAIT,                        RES3
   => HASMORE, Row 6               RES3
   => WAITING,                     RES3
   => DONE, no output!             RES3
    */

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(stats.getFiltered() == 1);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
