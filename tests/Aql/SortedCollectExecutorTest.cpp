////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "AqlExecutorTestCase.h"
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
#include <velocypack/Options.h>
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

  RegIdSet const regToClear = {};
  RegIdSetStack const regToKeep = {{}};
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  std::vector<std::string> aggregateTypes;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;

  // if count = true, then we need to set a countRegister
  RegisterId collectRegister;
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  RegisterInfos registerInfos;
  SortedCollectExecutorInfos executorInfos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  SortedCollectExecutorTestNoRowsUpstream()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        server(),
        fakedQuery(server.createFakeQuery()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(RegisterPlan::MaxRegisterId),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        registerInfos(RegIdSet{0}, RegIdSet{1}, 1 /*nrIn*/, 2 /*nrOut*/, regToClear, regToKeep),
        executorInfos(std::move(groupRegisters), collectRegister, expressionRegister,
                      expressionVariable, std::move(aggregateTypes), std::move(variables),
                      std::move(aggregateRegisters), &VPackOptions::Defaults, count),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {}
};

TEST_F(SortedCollectExecutorTestNoRowsUpstream, producer_gets_empty_input) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(1, registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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

  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  RegisterId collectRegister;

  RegisterId nrOutputRegister;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  std::vector<std::string> aggregateTypes;

  // if count = true, then we need to set a valid countRegister
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  RegisterInfos registerInfos;
  SortedCollectExecutorInfos executorInfos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  SortedCollectExecutorTestRowsUpstream()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        server(),
        fakedQuery(server.createFakeQuery()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(2),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        registerInfos(RegIdSet{0}, RegIdSet{1, 2}, 1 /*nrIn*/, 3 /*nrOut*/,
                      RegIdFlatSet{}, RegIdFlatSetStack{{}}),
        executorInfos(std::move(groupRegisters), collectRegister, expressionRegister,
                      expressionVariable, std::move(aggregateTypes), std::move(variables),
                      std::move(aggregateRegisters), &VPackOptions::Defaults, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(SortedCollectExecutorTestRowsUpstream, producer_1) {
  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, executorInfos);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(2, registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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
  SortedCollectExecutor testee(fetcher, executorInfos);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}, {3}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->numRows(),
                                    registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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
    ASSERT_EQ(inputBlock->numRows(), result.numRowsWritten());
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
  SortedCollectExecutor testee(fetcher, executorInfos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{1}, {1}, {2}, {2}, {3}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->numRows(),
                                    registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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
  SortedCollectExecutor testee(fetcher, executorInfos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{1}, {1}, {2}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->numRows(),
                                    registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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

  RegIdSet regToClear = {};
  RegIdSetStack regToKeep = {{}};
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters = {{1, 0}};

  auto readableInputRegisters = RegIdSet{0};
  auto writeableOutputRegisters = RegIdSet{1, 2};
  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

  std::vector<std::string> aggregateTypes = {"SUM"};

  RegisterId collectRegister = RegisterPlan::MaxRegisterId;
  RegisterId expressionRegister = RegisterPlan::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;

  auto registerInfos = RegisterInfos(std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters), 1, nrOutputRegister,
                                     std::move(regToClear), std::move(regToKeep));
  auto executorInfos =
      SortedCollectExecutorInfos(std::move(groupRegisters), collectRegister,
                                 expressionRegister, expressionVariable,
                                 std::move(aggregateTypes), std::move(variables),
                                 std::move(aggregateRegisters),
                                 &VPackOptions::Defaults, false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->numRows(),
                                    registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, executorInfos);

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

  RegIdSet regToClear;
  RegIdSetStack regToKeep = {{}};
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

  auto readableInputRegisters = RegIdSet{0};

  auto writeableOutputRegisters = RegIdSet{1, 2};

  RegisterId nrOutputRegister = 3;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

  std::vector<std::string> aggregateTypes;
  aggregateTypes.emplace_back("LENGTH");

  RegisterId collectRegister = RegisterPlan::MaxRegisterId;
  RegisterId expressionRegister = RegisterPlan::MaxRegisterId;
  Variable const* expressionVariable = nullptr;
  std::vector<std::pair<std::string, RegisterId>> variables;

  auto registerInfos = RegisterInfos(std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters), 1,
                                     nrOutputRegister, regToClear, regToKeep);
  auto executorInfos =
      SortedCollectExecutorInfos(std::move(groupRegisters), collectRegister,
                                 expressionRegister, expressionVariable,
                                 std::move(aggregateTypes), std::move(variables),
                                 std::move(aggregateRegisters),
                                 &VPackOptions::Defaults, false);

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};

  auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  SortedCollectExecutor testee(fetcher, executorInfos);

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{"\"a\""}, {"\"aa\""}, {"\"aaa\""}});
  AqlCall clientCall;

  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr outputBlock =
      itemBlockManager.requestBlock(inputBlock->numRows(),
                                    registerInfos.numberOfOutputRegisters());
  OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear(), {});

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

  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
  RegisterId collectRegister;

  RegisterId nrOutputRegister;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  std::vector<std::string> aggregateTypes;

  // if count = true, then we need to set a valid countRegister
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  RegisterInfos registerInfos;
  SortedCollectExecutorInfos executorInfos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  SortedCollectExecutorTestSkip()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(2),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        registerInfos(RegIdSet{0}, RegIdSet{1, 2}, 1, nrOutputRegister,
                      RegIdFlatSet{}, RegIdFlatSetStack{{}}),
        executorInfos(std::move(groupRegisters), collectRegister, expressionRegister,
                      expressionVariable, std::move(aggregateTypes), std::move(variables),
                      std::move(aggregateRegisters), &VPackOptions::Defaults, count),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)) {}
};

TEST_F(SortedCollectExecutorTestSkip, skip_1) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, std::make_shared<VPackBuffer<uint8_t>>(), false);

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}, {2}});
  AqlItemBlockInputRange emptyInputRange(ExecutorState::HASMORE);
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;
  clientCall.offset = 2;

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRange, clientCall);
    ASSERT_EQ(ExecutorState::HASMORE, state);
    ASSERT_FALSE(upstreamCall.hasHardLimit());
    ASSERT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    ASSERT_EQ(0, upstreamCall.offset);
    ASSERT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    ASSERT_EQ(skipped, 0);
  }
  clientCall.resetSkipCount();

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange, clientCall);
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
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }
  clientCall.resetSkipCount();

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
  clientCall.resetSkipCount();

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->numRows(),
                                      registerInfos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                            registerInfos.registersToKeep(),
                            registerInfos.registersToClear(), clientCall);

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
  AqlItemBlockInputRange inputRange(ExecutorState::HASMORE, 0, inputBlock, 0);
  AqlItemBlockInputRange emptyInputRangeDone(ExecutorState::DONE);

  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }
  clientCall.resetSkipCount();

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
  clientCall.resetSkipCount();

  {
    auto [state, stats, skipped, upstreamCall] =
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
  AqlItemBlockInputRange inputRange(ExecutorState::HASMORE, 0, inputBlock, 0);

  SharedAqlItemBlockPtr inputBlock2 = buildBlock<1>(itemBlockManager, {{2}});
  AqlItemBlockInputRange inputRange2(ExecutorState::HASMORE, 0, inputBlock2, 0);
  AqlItemBlockInputRange emptyInputRangeDone(ExecutorState::DONE);

  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }
  clientCall.resetSkipCount();

  {
    // 1, 1
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
  clientCall.resetSkipCount();

  {
    // 2
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange2, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
  clientCall.resetSkipCount();

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->numRows(),
                                      registerInfos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                            registerInfos.registersToKeep(),
                            registerInfos.registersToClear(), clientCall);

    auto [state, stats, upstreamCall] = testee.produceRows(inputRange2, result);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(result.numRowsWritten(), 0);
    EXPECT_FALSE(result.produced());
  }
  clientCall.resetSkipCount();

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->numRows(),
                                      registerInfos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                            registerInfos.registersToKeep(),
                            registerInfos.registersToClear(), clientCall);

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
  AqlItemBlockInputRange inputRange(ExecutorState::DONE, 0, inputBlock, 0);

  SortedCollectExecutor testee(fetcher, executorInfos);

  AqlCall clientCall;
  clientCall.offset = 1;

  {
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(emptyInputRange, clientCall);
    EXPECT_EQ(ExecutorState::HASMORE, state);
    EXPECT_FALSE(upstreamCall.hasHardLimit());
    EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(upstreamCall.softLimit));
    EXPECT_EQ(0, upstreamCall.offset);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 0);
  }
  clientCall.resetSkipCount();

  {
    // 1, 1, 2
    auto [state, stats, skipped, upstreamCall] =
        testee.skipRowsRange(inputRange, clientCall);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(clientCall.fullCount, upstreamCall.fullCount);
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(inputRange.upstreamState(), ExecutorState::HASMORE);
  }
  clientCall.resetSkipCount();

  {
    SharedAqlItemBlockPtr outputBlock =
        itemBlockManager.requestBlock(inputBlock->numRows(),
                                      registerInfos.numberOfOutputRegisters());
    OutputAqlItemRow result(outputBlock, registerInfos.getOutputRegisters(),
                            registerInfos.registersToKeep(),
                            registerInfos.registersToClear(), clientCall);

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
  clientCall.resetSkipCount();
}

using SortedCollectTestHelper = ExecutorTestHelper<1, 1>;
using SortedCollectSplitType = SortedCollectTestHelper::SplitType;

class SortedCollectExecutorTestSplit
    : public AqlExecutorTestCaseWithParam<std::tuple<SortedCollectSplitType>> {
 protected:
  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;

  RegisterId collectRegister;
  RegisterId nrOutputRegister;

  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  std::vector<std::string> aggregateTypes;

  // if count = true, then we need to set a valid countRegister
  RegisterId expressionRegister;
  Variable const* expressionVariable;
  std::vector<std::pair<std::string, RegisterId>> variables;
  bool count;

  RegisterInfos registerInfos;
  SortedCollectExecutorInfos executorInfos;

  SortedCollectExecutorTestSplit()
      : groupRegisters{std::make_pair<RegisterId, RegisterId>(1, 0)},
        collectRegister(2),
        nrOutputRegister(3),
        expressionRegister(RegisterPlan::MaxRegisterId),
        expressionVariable(nullptr),
        count(false),
        registerInfos(RegIdSet{0}, RegIdSet{1, 2}, 1, nrOutputRegister,
                      RegIdFlatSet{}, RegIdFlatSetStack{{}}),
        executorInfos(std::move(groupRegisters), collectRegister, expressionRegister,
                      expressionVariable, std::move(aggregateTypes),
                      std::move(variables), std::move(aggregateRegisters),
                      &VPackOptions::Defaults, count) {}
};

TEST_P(SortedCollectExecutorTestSplit, split_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<SortedCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{2, AqlCall::Infinity{}, 2u, true})
      .expectOutputValueList(3, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortedCollectExecutorTestSplit, split_2) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<SortedCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{2, 2u, AqlCall::Infinity{}, false})
      .expectOutputValueList(3, 4)
      .expectSkipped(2)
      .expectedState(ExecutionState::HASMORE)
      .run();
}

TEST_P(SortedCollectExecutorTestSplit, split_3) {
  auto [split] = GetParam();

  makeExecutorTestHelper()
      .addConsumer<SortedCollectExecutor>(std::move(registerInfos), std::move(executorInfos))
      .setInputValueList(1, 2, 3, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{1, AqlCall::Infinity{}, 10u, true})
      .expectOutputValueList(2, 3, 4, 5)
      .expectSkipped(1)
      .expectedState(ExecutionState::DONE)
      .run();
}

template <size_t... vs>
const SortedCollectSplitType splitIntoBlocks =
    SortedCollectSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const SortedCollectSplitType splitStep = SortedCollectSplitType{step};

INSTANTIATE_TEST_CASE_P(SortedCollectExecutor, SortedCollectExecutorTestSplit,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>));

}  // namespace arangodb::tests::aql
