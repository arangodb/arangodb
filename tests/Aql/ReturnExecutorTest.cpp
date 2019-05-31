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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class ReturnExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> registersToKeep;
  RegisterId inputRegister;

  ReturnExecutorTest()
      : itemBlockManager(&monitor),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        registersToKeep(make_shared_unordered_set()),
        inputRegister(0) {}
};

TEST_F(ReturnExecutorTest, NoRowsUpstreamProducerDoesNotWait) {
  ReturnExecutorInfos infos(inputRegister, 1 /*nr in*/, 1 /*nr out*/, true /*do count*/);
  auto& outputRegisters = infos.getOutputRegisters();
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(input.steal(), false);
  ReturnExecutor testee(fetcher, infos);
  CountStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(ReturnExecutorTest, NoRowsUpstreamProducerWaits) {
  ReturnExecutorInfos infos(inputRegister, 1 /*nr in*/, 1 /*nr out*/, true /*do count*/);
  auto& outputRegisters = infos.getOutputRegisters();
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(input.steal(), true);
  ReturnExecutor testee(fetcher, infos);
  CountStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(ReturnExecutorTest, RowsUpstreamProducerDoesNotWait) {
  ReturnExecutorInfos infos(inputRegister, 1 /*nr in*/, 1 /*nr out*/, true /*do count*/);
  auto& outputRegisters = infos.getOutputRegisters();
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(input->buffer(), false);
  ReturnExecutor testee(fetcher, infos);
  CountStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

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

TEST_F(ReturnExecutorTest, RowsUpstreamProducerWaits) {
  ReturnExecutorInfos infos(inputRegister, 1 /*nr in*/, 1 /*nr out*/, true /*do count*/);
  auto& outputRegisters = infos.getOutputRegisters();
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  ReturnExecutor testee(fetcher, infos);
  CountStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!row.produced());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
