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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortedCollectExecutor.h"
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

class SortedCollectExecutorTestNoRowsUpstream : public ::testing::Test {
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

  std::vector<std::string> aggregateTypes;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;

  // if count = true, then we need to set a countRegister
  RegisterId collectRegister;
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  std::unordered_set<RegisterId> readableInputRegisters;
  std::unordered_set<RegisterId> writeableOutputRegisters;

  SortedCollectExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  SortedCollectExecutorTestNoRowsUpstream()
      : itemBlockManager(&monitor),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(ExecutionNode::MaxRegisterId),
        expressionRegister(ExecutionNode::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        readableInputRegisters{0},
        writeableOutputRegisters{1},
        infos(1 /*nrIn*/, 2 /*nrOut*/, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::move(groupRegisters), collectRegister, expressionRegister,
              expressionVariable, std::move(aggregateTypes),
              std::move(variables), std::move(aggregateRegisters), trx, count),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {}
};

TEST_F(SortedCollectExecutorTestNoRowsUpstream, producer_doesnt_wait) {
  SingleRowFetcherHelper<false> fetcher(input.steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(SortedCollectExecutorTestNoRowsUpstream, producer_waits) {
  SingleRowFetcherHelper<false> fetcher(input.steal(), true);
  SortedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

class SortedCollectExecutorTestRowsUpstream : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;

  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  std::unordered_set<RegisterId> readableInputRegisters;

  RegisterId collectRegister;

  std::unordered_set<RegisterId> writeableOutputRegisters;

  RegisterId nrOutputRegister;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  std::vector<std::string> aggregateTypes;

  // if count = true, then we need to set a valid countRegister
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  SortedCollectExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  SortedCollectExecutorTestRowsUpstream()
      : itemBlockManager(&monitor),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        readableInputRegisters({0}),
        collectRegister(2),
        writeableOutputRegisters({1, 2}),
        nrOutputRegister(3),
        expressionRegister(ExecutionNode::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        infos(1, nrOutputRegister, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::move(groupRegisters), collectRegister, expressionRegister,
              expressionVariable, std::move(aggregateTypes),
              std::move(variables), std::move(aggregateRegisters), trx, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_doesnt_wait) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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

  // check for groups in this executor they are guaranteed to be ordered

  // First group
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 1);
  // check for collect
  x = block->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 2);
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_doesnt_wait_2) {
  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();

  // check for collects
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 1);

  x = block->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 2);

  x = block->getValue(2, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 3);
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_doesnt_wait_3) {
  // Input order needs to be guaranteed
  auto input = VPackParser::fromJson("[ [1], [1], [2], [2], [3] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  // After done return done
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 1);

  x = block->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 2);

  x = block->getValue(2, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 3);
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_doesnt_wait_4) {
  auto input = VPackParser::fromJson("[ [1], [1], [2], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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

  // After DONE return DONE
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto block = result.stealBlock();

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 1);

  x = block->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 2);
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_waits) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  SortedCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

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

  // check for types
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 1);

  x = block->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_TRUE(x.slice().getInt() == 2);
}

TEST(SortedCollectExecutorTestRowsUpstreamCount, test) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));
  writeableOutputRegisters.insert(2);

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("SUM");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = ExecutionNode::MaxRegisterId;
  RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;

  SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   expressionRegister, expressionVariable,
                                   std::move(aggregateTypes), std::move(variables),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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

  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  EXPECT_TRUE(x.slice().getInt() == 1);

  // Check the SUM register
  AqlValue counter = newBlock->getValue(0, 2);
  ASSERT_TRUE(counter.isNumber());
  EXPECT_TRUE(counter.slice().getDouble() == 1);

  // check for types
  x = newBlock->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  EXPECT_TRUE(x.slice().getInt() == 2);

  // Check the SUM register
  counter = newBlock->getValue(1, 2);
  ASSERT_TRUE(counter.isNumber());
  EXPECT_TRUE(counter.slice().getDouble() == 2);
}

TEST(SortedCollectExecutorTestRowsUpstreamCountNumbers, test) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = ExecutionNode::MaxRegisterId;
  RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;
  writeableOutputRegisters.insert(2);

  SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   expressionRegister, expressionVariable,
                                   std::move(aggregateTypes), std::move(variables),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  EXPECT_TRUE(x.slice().getInt() == 1);

  // Check the LENGTH register
  AqlValue xx = newBlock->getValue(0, 2);
  ASSERT_TRUE(xx.isNumber());
  EXPECT_TRUE(xx.slice().getInt() == 1);

  // check for types
  x = newBlock->getValue(1, 1);
  ASSERT_TRUE(x.isNumber());
  EXPECT_TRUE(x.slice().getInt() == 2);

  // Check the LENGTH register
  xx = newBlock->getValue(1, 2);
  ASSERT_TRUE(xx.isNumber());
  EXPECT_TRUE(xx.slice().getInt() == 1);

  // check for types
  x = newBlock->getValue(2, 1);
  ASSERT_TRUE(x.isNumber());
  EXPECT_TRUE(x.slice().getInt() == 3);

  // Check the LENGTH register
  xx = newBlock->getValue(2, 2);
  ASSERT_TRUE(xx.isNumber());
  EXPECT_TRUE(xx.slice().getInt() == 1);
}

TEST(SortedCollectExecutorTestRowsUpstreamCountStrings, test) {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear;
  std::unordered_set<RegisterId> regToKeep;
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  std::unordered_set<RegisterId> readableInputRegisters;
  readableInputRegisters.insert(0);

  std::unordered_set<RegisterId> writeableOutputRegisters;
  writeableOutputRegisters.insert(1);

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  // if count = true, then we need to set a valid countRegister
  bool count = true;
  RegisterId collectRegister = ExecutionNode::MaxRegisterId;
  RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;
  writeableOutputRegisters.insert(2);

  SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   expressionRegister, expressionVariable,
                                   std::move(aggregateTypes), std::move(variables),
                                   std::move(aggregateRegisters), trx, count);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
  NoStats stats{};

  auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

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
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());

  std::vector<std::string> myStrings;
  std::vector<int> myCountNumbers;
  auto newBlock = result.stealBlock();

  // check for types
  AqlValue x = newBlock->getValue(0, 1);
  ASSERT_TRUE(x.isString());
  EXPECT_TRUE(x.slice().copyString() == "a");

  // Check the count register
  AqlValue c = newBlock->getValue(0, 2);
  ASSERT_TRUE(c.isNumber());
  EXPECT_TRUE(c.slice().getInt() == 1);

  // check for types
  x = newBlock->getValue(1, 1);
  ASSERT_TRUE(x.isString());
  EXPECT_TRUE(x.slice().copyString() == "aa");

  // Check the count register
  c = newBlock->getValue(1, 2);
  ASSERT_TRUE(c.isNumber());
  EXPECT_TRUE(c.slice().getInt() == 1);

  // check for types
  x = newBlock->getValue(2, 1);
  ASSERT_TRUE(x.isString());
  EXPECT_TRUE(x.slice().copyString() == "aaa");

  // Check the count register
  c = newBlock->getValue(2, 2);
  ASSERT_TRUE(c.isNumber());
  EXPECT_TRUE(c.slice().getInt() == 1);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
