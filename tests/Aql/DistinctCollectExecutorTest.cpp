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
#include "Aql/Collection.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "tests/Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class DistinctCollectExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  std::unordered_set<RegisterId> readableInputRegisters;
  std::unordered_set<RegisterId> writeableOutputRegisters;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  DistinctCollectExecutorTest()
      : itemBlockManager(&monitor),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()) {}
};

TEST_F(DistinctCollectExecutorTest, if_no_rows_in_upstream_the_producer_doesnt_wait) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 2));
  DistinctCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, 2));

  SingleRowFetcherHelper<false> fetcher(input.steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(DistinctCollectExecutorTest, if_no_rows_in_upstream_the_producer_waits) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 2));
  DistinctCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, 2));

  SingleRowFetcherHelper<false> fetcher(input.steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_no_distinct_values_the_producer_doesnt_wait) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

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

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.toInt64() == 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_TRUE(z.toInt64() == 2);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_no_distinct_values_the_producer_waits) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.toInt64() == 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_TRUE(z.toInt64() == 2);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_with_distinct_values_the_producer_doesnt_wait) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

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
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.toInt64() == 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_TRUE(z.toInt64() == 2);

  AqlValue y = block->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  ASSERT_TRUE(y.toInt64() == 3);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_with_distinct_values_the_producer_waits) {
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.toInt64() == 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_TRUE(z.toInt64() == 2);

  AqlValue y = block->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  ASSERT_TRUE(y.toInt64() == 3);
}

}  // namespace tests
}  // namespace arangodb
}  // namespace arangodb
