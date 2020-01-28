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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SortedCollectExecutor.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

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
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(RegisterPlan::MaxRegisterId),
        expressionRegister(RegisterPlan::MaxRegisterId),
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
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(1, infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }
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
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        readableInputRegisters({0}),
        collectRegister(2),
        writeableOutputRegisters({1, 2}),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        infos(1, nrOutputRegister, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::move(groupRegisters), collectRegister, expressionRegister,
              expressionVariable, std::move(aggregateTypes),
              std::move(variables), std::move(aggregateRegisters), trx, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_1) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(2, infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(2, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 1);
    x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 2);
  }
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_2) {
  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}, {3}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(inputBlock->size(), result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 1);
    x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 2);
    x = outputBlock->getValue(2, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 3);
  }
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_3) {
  // Input order needs to be guaranteed
  auto input = VPackParser::fromJson("[ [1], [1], [2], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{1}, {1}, {2}, {2}, {3}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(3, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 1);
    x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 2);
    x = outputBlock->getValue(2, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 3);
  }
}

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_4) {
  auto input = VPackParser::fromJson("[ [1], [1], [2], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{1}, {1}, {2}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(2, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 1);
    x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 2);
  }
}

TEST(SortedCollectExecutorTestRowsUpstreamCount, test) {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
  arangodb::transaction::Methods* trx = fakedQuery->trx();

  std::unordered_set<RegisterId> regToClear = {};
  std::unordered_set<RegisterId> regToKeep = {};
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters = {{1, 0}};

  std::unordered_set<RegisterId> readableInputRegisters = {0};
  std::unordered_set<RegisterId> writeableOutputRegisters = {1, 2};
  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

  std::vector<std::string> aggregateTypes = {"SUM"};

  RegisterId collectRegister = RegisterPlan::MaxRegisterId;
  RegisterId expressionRegister = RegisterPlan::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;

  SortedCollectExecutorInfos infos(1, nrOutputRegister, std::move(regToClear),
                                   std::move(regToKeep), std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   expressionRegister, expressionVariable,
                                   std::move(aggregateTypes), std::move(variables),
                                   std::move(aggregateRegisters), trx, false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(2, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 1);
    x = outputBlock->getValue(0, 2);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getDouble(), 1);

    x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getInt(), 2);
    x = outputBlock->getValue(1, 2);
    ASSERT_TRUE(x.isNumber());
    ASSERT_EQ(x.slice().getDouble(), 2);
  }
}

TEST(SortedCollectExecutorTestRowsUpstreamCountStrings, test) {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

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

  RegisterId collectRegister = RegisterPlan::MaxRegisterId;
  RegisterId expressionRegister = RegisterPlan::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;
  writeableOutputRegisters.insert(2);

  SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                   std::move(readableInputRegisters),
                                   std::move(writeableOutputRegisters),
                                   std::move(groupRegisters), collectRegister,
                                   expressionRegister, expressionVariable,
                                   std::move(aggregateTypes), std::move(variables),
                                   std::move(aggregateRegisters), trx, false);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};

  auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{"\"a\""}, {"\"aa\""}, {"\"aaa\""}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear(), {});

  {
    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRange, result);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(0, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(3, result.numRowsWritten());
    ASSERT_FALSE(result.produced());
  }

  {
    AqlValue x = outputBlock->getValue(0, 1);
    ASSERT_TRUE(x.isString());
    ASSERT_TRUE(x.slice().isEqualString("a"));

    AqlValue c = outputBlock->getValue(0, 2);
    ASSERT_TRUE(c.isNumber());
    EXPECT_EQ(c.slice().getInt(), 1);
  }

  {
    AqlValue x = outputBlock->getValue(1, 1);
    ASSERT_TRUE(x.isString());
    ASSERT_TRUE(x.slice().isEqualString("aa"));

    AqlValue c = outputBlock->getValue(1, 2);
    ASSERT_TRUE(c.isNumber());
    EXPECT_EQ(c.slice().getInt(), 1);
  }

  {
    AqlValue x = outputBlock->getValue(2, 1);
    ASSERT_TRUE(x.isString());
    ASSERT_TRUE(x.slice().isEqualString("aaa"));

    AqlValue c = outputBlock->getValue(2, 2);
    ASSERT_TRUE(c.isNumber());
    EXPECT_EQ(c.slice().getInt(), 1);
  }
}

class SortedCollectExecutorTestSkip : public ::testing::Test {
 protected:
  // ExecutionState state;
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

  SortedCollectExecutorTestSkip()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        readableInputRegisters({0}),
        collectRegister(2),
        writeableOutputRegisters({1, 2}),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        infos(1, nrOutputRegister, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::move(groupRegisters), collectRegister, expressionRegister,
              expressionVariable, std::move(aggregateTypes),
              std::move(variables), std::move(aggregateRegisters), trx, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(SortedCollectExecutorTestSkip, skip_1) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;
  clientCall.offset = 2;

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(emptyInputRange, clientCall);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(skipped, 0);
  }

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange, clientCall);
    ASSERT_EQ(ExecutorState::DONE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(skipped, 2);
  }
}

TEST_F(SortedCollectExecutorTestSkip, skip_2) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear(), clientCall);

    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    EXPECT_EQ(ExecutorState::DONE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(1, result.numRowsWritten());
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::DONE);

    {
      AqlValue x = outputBlock->getValue(0, 1);
      EXPECT_TRUE(x.isNumber());
      EXPECT_EQ(x.slice().getInt(), 2);
    }
  }
}

TEST_F(SortedCollectExecutorTestSkip, skip_3) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {1}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::HASMORE, inputBlock, 0,
                                    inputBlock->size());
  AqlItemBlockInputRange emptyInputRangeDone(ExecutorState::DONE);

  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }

  {
    auto [state, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRangeDone, clientCall);
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
}

TEST_F(SortedCollectExecutorTestSkip, skip_4) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {1}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::HASMORE, inputBlock, 0,
                                    inputBlock->size());

  SharedAqlItemBlockPtr inputBlock2 = buildBlock<1>(itemBlockManager, {{2}});
  AqlItemBlockInputRange inputRange2(ExecutorState::HASMORE, inputBlock2, 0,
                                     inputBlock2->size());
  AqlItemBlockInputRange emptyInputRangeDone(ExecutorState::DONE);

  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }

  {
    // 1, 1
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }

  {
    // 2
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange2, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear(), clientCall);

    auto [state, stats, upstreamCall] = testee.produceRows(inputRange2, result);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(result.numRowsWritten(), 0);
    EXPECT_FALSE(result.produced());
  }

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear(), clientCall);

    auto [state, stats, upstreamCall] = testee.produceRows(emptyInputRangeDone, result);
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(1, result.numRowsWritten());

    {
      AqlValue x = outputBlock->getValue(0, 1);
      EXPECT_TRUE(x.isNumber());
      EXPECT_EQ(x.slice().getInt(), 2);
    }
  }
}

TEST_F(SortedCollectExecutorTestSkip, skip_5) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {1}, {2}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                    inputBlock->size());

  SortedCollectExecutor testee(fetcher, infos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }

  {
    // 1, 1, 2
    auto [state, skipped, upstreamCall] = testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->size(), infos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear(), clientCall);

    auto [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
    EXPECT_EQ(ExecutorState::DONE, state);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(1, result.numRowsWritten());

    {
      AqlValue x = outputBlock->getValue(0, 1);
      EXPECT_TRUE(x.isNumber());
      EXPECT_EQ(x.slice().getInt(), 2);
    }
  }
}

class SortedCollectExecutorTestSplit : public ::testing::Test {
 protected:
  // ExecutionState state;
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

  SortedCollectExecutorTestSplit()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        trx(fakedQuery->trx()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        readableInputRegisters({0}),
        collectRegister(2),
        writeableOutputRegisters({1, 2}),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        infos(1, nrOutputRegister, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::move(groupRegisters), collectRegister, expressionRegister,
              expressionVariable, std::move(aggregateTypes),
              std::move(variables), std::move(aggregateRegisters), trx, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}

  struct TestData {
    SortedCollectExecutorTestSplit& test;
    SortedCollectExecutor testee;
    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher;

    std::vector<uint64_t> values = {};
    std::vector<uint64_t> output = {};
    std::vector<size_t> split = {};
    uint64_t splitStep = 0;
    size_t skip = 0;
    size_t consume = 0;
    std::optional<ExecutorState> endState = {};

    template <typename... Vs>
    TestData& setValues(Vs&&... vs) {
      values = {uint64_t(vs)...};
      return *this;
    }

    template <typename... Vs>
    TestData& setSplit(Vs&&... vs) {
      split = {size_t(vs)...};
      return *this;
    }

    TestData& setSplitStep(size_t step) {
      splitStep = step;
      return *this;
    }

    TestData& setLimit(size_t skip, size_t consume) {
      this->skip = skip;
      this->consume = consume;
      return *this;
    }

    template <typename... Vs>
    TestData& expectOutput(Vs&&... vs) {
      output = {uint64_t(vs)...};
      return *this;
    }

    TestData& expectEndState(ExecutorState state) {
      endState = state;
      return *this;
    }

    void run() {
      ASSERT_FALSE(splitStep == 0 && split.empty());
      ASSERT_FALSE(output.empty());

      std::vector<std::pair<SharedAqlItemBlockPtr, AqlItemBlockInputRange>> inputs;
      if (splitStep != 0) {
        MatrixBuilder<1> matrix;
        for (auto const value : values) {
          matrix.push_back({value});

          if (matrix.size() == splitStep) {
            SharedAqlItemBlockPtr inputBlock =
                buildBlock<1>(test.itemBlockManager, std::move(matrix));
            AqlItemBlockInputRange inputRange(ExecutorState::HASMORE,
                                              inputBlock, 0, inputBlock->size());
            inputs.emplace_back(std::move(inputBlock), std::move(inputRange));
            matrix.clear();
          }
        }
        if (matrix.empty()) {
          inputs.emplace_back(nullptr, AqlItemBlockInputRange{ExecutorState::DONE});
        } else {
          SharedAqlItemBlockPtr inputBlock =
              buildBlock<1>(test.itemBlockManager, std::move(matrix));
          AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                            inputBlock->size());
          inputs.emplace_back(std::move(inputBlock), std::move(inputRange));
        }
      } else if (!split.empty()) {
        MatrixBuilder<1> matrix;
        auto splitIter = split.begin();

        for (auto const value : values) {
          matrix.push_back({value});

          if (splitIter != split.end() && matrix.size() == *splitIter) {
            SharedAqlItemBlockPtr inputBlock =
                buildBlock<1>(test.itemBlockManager, std::move(matrix));
            AqlItemBlockInputRange inputRange(ExecutorState::HASMORE,
                                              inputBlock, 0, inputBlock->size());
            inputs.emplace_back(std::move(inputBlock), std::move(inputRange));
            matrix.clear();
            splitIter++;
          }
        }
        if (matrix.empty()) {
          inputs.emplace_back(nullptr, AqlItemBlockInputRange{ExecutorState::DONE});
        } else {
          SharedAqlItemBlockPtr inputBlock =
              buildBlock<1>(test.itemBlockManager, std::move(matrix));
          AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                            inputBlock->size());
          inputs.emplace_back(std::move(inputBlock), std::move(inputRange));
        }
      } else {
        ASSERT_TRUE(false);
      }

      AqlCall clientCall;
      clientCall.offset = skip;

      SharedAqlItemBlockPtr outputBlock =
          test.itemBlockManager.requestBlock(output.size(),
                                             test.infos.numberOfOutputRegisters());

      OutputAqlItemRow result{outputBlock, test.infos.getOutputRegisters(),
                              test.infos.registersToKeep(),
                              test.infos.registersToClear(), clientCall};

      AqlItemBlockInputRange initInputRange(ExecutorState::HASMORE);
      // init with empty input range
      if (skip > 0) {
        auto [state, skipped, upstreamCall] =
            testee.skipRowsRange(initInputRange, result.getModifiableClientCall());
        EXPECT_EQ(ExecutorState::HASMORE, state);
        EXPECT_EQ(skipped, 0);
      } else {
        auto [state, stats, upstreamCall] = testee.produceRows(initInputRange, result);
        ASSERT_EQ(ExecutorState::HASMORE, state);
        ASSERT_FALSE(result.produced());
      }

      size_t didSkip = 0;

      ExecutorState state = ExecutorState::HASMORE;
      bool wasDone = false;

      for (auto [block, itemRange] : inputs) {
        /*
         * Call skip until didskip == skip
         * Call produce until numItems == consume
         */

        EXPECT_EQ(state, ExecutorState::HASMORE);

        if (didSkip < skip) {
          ASSERT_EQ(result.getClientCall().offset, skip - didSkip);
          AqlCall upstreamCall;
          size_t skipped;
          std::tie(state, skipped, upstreamCall) =
              testee.skipRowsRange(itemRange, result.getModifiableClientCall());
          ASSERT_LE(didSkip + skipped, skip);
          ASSERT_FALSE(wasDone && state == ExecutorState::HASMORE);
          wasDone = state == ExecutorState::DONE;
          didSkip += skipped;
        } else if (consume == 0 || result.numRowsWritten() < consume) {
          EXPECT_EQ(result.getClientCall().offset, 0);
          AqlCall upstreamCall;
          SortedCollectExecutor::Stats stats;
          std::tie(state, stats, upstreamCall) = testee.produceRows(itemRange, result);
          ASSERT_FALSE(wasDone && state == ExecutorState::HASMORE);
          wasDone = state == ExecutorState::DONE;
        } else {
          ASSERT_TRUE(false);
          break;
        }
      }

      if (endState) {
        EXPECT_EQ(state, endState.value());
      }

      {}
    }
  };

  [[nodiscard]] TestData newTest() {
    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
        itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

    SortedCollectExecutor testee(fetcher, infos);
    return {*this, std::move(testee), fetcher};
  }
};

template <typename E, std::size_t inputColumns = 1, std::size_t outputColumns = 1>
struct ExecutorTestHelper {
  using SplitType = std::variant<std::vector<std::size_t>, std::size_t, std::monostate>;

  ExecutorTestHelper(ExecutorTestHelper const&) = delete;
  ExecutorTestHelper(ExecutorTestHelper&&) = delete;
  explicit ExecutorTestHelper(E& testee) : testee(testee) {}

  auto setCall(AqlCall c) {
    call = c;
    return *this;
  }

  auto setInputValue(MatrixBuilder<inputColumns> in) -> ExecutorTestHelper& {
    input = std::move(in);
    return *this;
  }

  template <typename... Ts>
  auto setInputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    input = MatrixBuilder<inputColumns>{{ts}...};
    return *this;
  }

  auto setInputSplit(std::vector<std::size_t> const& list) -> ExecutorTestHelper& {
    inputSplit = list;
    return *this;
  }

  auto setInputSplitStep(std::size_t step) -> ExecutorTestHelper& {
    inputSplit = step;
    return *this;
  }

  template <typename T>
  auto setOutputSplit(T&& list) -> ExecutorTestHelper& {
    ASSERT_FALSE(true);
    outputSplit = std::forward<T>(list);
    return *this;
  }

  auto expectOutput(std::array<std::size_t, outputColumns> const& regs,
                    MatrixBuilder<outputColumns> const& out) -> ExecutorTestHelper& {
    outputRegisters = regs;
    output = out;
    return *this;
  }

  template <typename... Ts>
  auto expectOutputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    static_assert(outputColumns == 1);
    outputRegisters[0] = 1;
    output = MatrixBuilder<outputColumns>{{ts}...};
    return *this;
  }

  auto expectFullCount(std::size_t count) -> ExecutorTestHelper& {
    fullCount = count;
    return *this;
  }

  void run(ExecutorInfos const& infos) {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager(&monitor, SerializationFormat::SHADOWROWS);

    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(output.size(), infos.numberOfOutputRegisters());

    OutputAqlItemRow result{outputBlock, infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear(), call};



    auto inputs = generateInputRanges(itemBlockManager);

    initCallExecutor(result);
    applyInputBlocks(inputs, result);

    SharedAqlItemBlockPtr expectedOutputBlock = buildBlock<outputColumns>(itemBlockManager, std::move(output));
    testOutputBlock(result, outputBlock, expectedOutputBlock);
  };

 private:
  void applyInputBlocks(std::vector<std::pair<SharedAqlItemBlockPtr, AqlItemBlockInputRange>>& inputs,
                        OutputAqlItemRow& result) {
    for (auto [block, itemRange] : inputs) {
      /*
       * Call skip until didskip == skip
       * Call produce until numItems == consume
       */
      AqlCall call = result.getClientCall();

      if (call.offset > 0) {
        std::ignore = testee.skipRowsRange(itemRange, result.getModifiableClientCall());
      } else {
        std::ignore = testee.produceRows(itemRange, result);
      }
    }
  }

  void testOutputBlock(OutputAqlItemRow& result, SharedAqlItemBlockPtr& outputBlock,
                       SharedAqlItemBlockPtr& expectedOutputBlock) {

    velocypack::Options vpackOptions;

    EXPECT_EQ(result.numRowsWritten(), output.size());
    for (size_t i = 0; i < result.numRowsWritten(); i++) {
      for (size_t j = 0; j < outputColumns; j++) {

        AqlValue x = outputBlock->getValue(i, outputRegisters[j]);
        AqlValue y = expectedOutputBlock->getValue(i, j);

        EXPECT_EQ(x.slice().toJson(), y.slice().toJson());

        EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0) << "Row " << i << " Column " << j << " (Reg " << outputRegisters[j] << ") do not agree";
      }
    }
  }

  void initCallExecutor(OutputAqlItemRow& result) {
    AqlItemBlockInputRange initInputRange(ExecutorState::HASMORE);
    // init with empty input range
    if (result.getModifiableClientCall().offset > 0) {
      auto [state, skipped, upstreamCall] =
          testee.skipRowsRange(initInputRange, result.getModifiableClientCall());
      EXPECT_EQ(ExecutorState::HASMORE, state);
      EXPECT_EQ(skipped, 0);
    } else {
      auto [state, stats, upstreamCall] = testee.produceRows(initInputRange, result);
      EXPECT_EQ(ExecutorState::HASMORE, state);
      EXPECT_FALSE(result.produced());
    }
  }

  auto generateInputRanges(AqlItemBlockManager& itemBlockManager)
      -> std::vector<std::pair<SharedAqlItemBlockPtr, AqlItemBlockInputRange>> {
    using VectorSizeT = std::vector<std::size_t>;

    std::vector<std::pair<SharedAqlItemBlockPtr, AqlItemBlockInputRange>> inputs;
    MatrixBuilder<inputColumns> matrix;

    std::optional<VectorSizeT::iterator> iter, end;

    if (std::holds_alternative<VectorSizeT>(inputSplit)) {
      iter = std::get<VectorSizeT>(inputSplit).begin();
      end = std::get<VectorSizeT>(inputSplit).end();
    }

    for (auto const& value : input) {
      matrix.push_back(value);

      bool openNewBlock =
          std::visit(overload{[&](std::vector<std::size_t>& list) {
                                if (*iter != *end && matrix.size() == **iter) {
                                  iter->operator++();
                                  return true;
                                }

                                return false;
                              },
                              [&](std::size_t size) {
                                return matrix.size() == size;
                              },
                              [](auto) { return false; }},
                     inputSplit);
      if (openNewBlock) {
        SharedAqlItemBlockPtr inputBlock =
            buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
        AqlItemBlockInputRange inputRange(ExecutorState::HASMORE, inputBlock, 0,
                                          inputBlock->size());
        inputs.emplace_back(std::move(inputBlock), std::move(inputRange));
        matrix.clear();
      }
    }

    SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, std::move(matrix));
    AqlItemBlockInputRange inputRange(ExecutorState::DONE, inputBlock, 0,
                                      inputBlock->size());
    inputs.emplace_back(std::move(inputBlock), std::move(inputRange));

    return inputs;
  }

  E& testee;
  AqlCall call;
  MatrixBuilder<inputColumns> input;
  MatrixBuilder<outputColumns> output;
  std::array<std::size_t, outputColumns> outputRegisters;
  std::optional<std::size_t> fullCount;

  SplitType inputSplit = {std::monostate()};
  SplitType outputSplit = {std::monostate()};
};

TEST_F(SortedCollectExecutorTestSplit, split_1) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SortedCollectExecutor executor(fetcher, infos);

  ExecutorTestHelper<SortedCollectExecutor>(executor)
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplit({2, 3})
      .setCall(/*...*/)
      .expectOutputValueList(3, 4)
      .expectFullCount(5)
      .outputSplit({1, 1})
      .run(infos);

  /*
  newTest <SortedCollectExecutor> ()
      .setValues(1, 1, 1, 2, 3, 4, 4, 5)
      .setSplit(2, 3)
      .setLimit(2, 2)
      .expectOutput(3, 4)
      .run();*/
}

TEST_F(SortedCollectExecutorTestSplit, split_2) {
  newTest()
      .setValues(1, 1, 1, 2, 3, 4, 4, 5)
      .setSplit(3, 3)
      .expectOutput(1, 2, 3, 4, 5)
      .run();
}

}  // namespace arangodb::tests::aql
