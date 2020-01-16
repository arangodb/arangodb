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

#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"
#include "TestEmptyExecutorHelper.h"
#include "TestExecutorHelper.h"
#include "TestLambdaExecutor.h"
#include "WaitingExecutionBlockMock.h"
#include "fakeit.hpp"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using LambdaExePassThrough = TestLambdaExecutor;
using LambdaExe = TestLambdaSkipExecutor;

// This test is supposed to only test getSome return values,
// it is not supposed to test the fetch logic!

class ExecutionBlockImplTest : public ::testing::Test {
 protected:
  // ExecutionState state
  SharedAqlItemBlockPtr result;

  // Mock of the ExecutionEngine
  fakeit::Mock<ExecutionEngine> mockEngine;
  ExecutionEngine& engine;

  // Mock of the AqlItemBlockManager
  fakeit::Mock<AqlItemBlockManager> mockBlockManager;
  AqlItemBlockManager& itemBlockManager;

  // Mock of the transaction
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx;

  // Mock of the transaction context
  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& context;

  // Mock of the Query
  fakeit::Mock<Query> mockQuery;
  Query& query;

  ExecutionState state;
  ResourceMonitor monitor;

  // Mock of the QueryOptions
  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions;
  ProfileLevel profile;

  // This is not used thus far in Base-Clase
  ExecutionNode const* node = nullptr;

  // Executor Infos
  TestExecutorHelperInfos infos;
  TestEmptyExecutorHelperInfos emptyInfos;

  SharedAqlItemBlockPtr block;

  ExecutionBlockImplTest()
      : engine(mockEngine.get()),
        itemBlockManager(mockBlockManager.get()),
        trx(mockTrx.get()),
        context(mockContext.get()),
        query(mockQuery.get()),
        lqueryOptions(mockQueryOptions.get()),
        profile(ProfileLevel(PROFILE_LEVEL_NONE)),
        node(nullptr),
        infos(0, 1, 1, {}, {0}),
        emptyInfos(0, 1, 1, {}, {0}),
        block(nullptr) {
    fakeit::When(Method(mockBlockManager, requestBlock)).AlwaysDo([&](size_t nrItems, RegisterId nrRegs) -> SharedAqlItemBlockPtr {
      return SharedAqlItemBlockPtr{new AqlItemBlock(itemBlockManager, nrItems, nrRegs)};
    });

    fakeit::When(Method(mockEngine, itemBlockManager)).AlwaysReturn(itemBlockManager);
    fakeit::When(Method(mockEngine, getQuery)).AlwaysReturn(&query);
    fakeit::When(OverloadedMethod(mockBlockManager, returnBlock, void(AqlItemBlock*&)))
        .AlwaysDo([&](AqlItemBlock*& block) -> void {
          AqlItemBlockManager::deleteBlock(block);
          block = nullptr;
        });
    fakeit::When(Method(mockBlockManager, resourceMonitor)).AlwaysReturn(&monitor);
    fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
        .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
    fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
        .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });
    fakeit::When(Method(mockQuery, trx)).AlwaysReturn(&trx);

    fakeit::When(Method(mockQueryOptions, getProfileLevel)).AlwaysReturn(profile);

    fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&context);
    fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&velocypack::Options::Defaults);
  }
};

TEST_F(ExecutionBlockImplTest,
       there_is_a_block_in_the_upstream_with_no_rows_inside_the_executor_waits_using_getsome) {
  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(block));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
  testee.addDependency(&dependency);

  size_t atMost = 1000;
  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_NE(block, nullptr);
  ASSERT_EQ(block->size(), 1);
  ASSERT_EQ(state, ExecutionState::DONE);

  // done should stay done!
  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(block, nullptr);
  ASSERT_EQ(state, ExecutionState::DONE);
}

TEST_F(ExecutionBlockImplTest,
       there_is_a_block_in_the_upstream_with_no_rows_inside_the_executor_waits_using_skipsome) {
  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(block));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
  testee.addDependency(&dependency);

  size_t atMost = 1;
  size_t skipped = 0;

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(skipped, 1);

  // done should stay done!
  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(skipped, 0);
}

TEST_F(ExecutionBlockImplTest,
       there_are_multiple_blocks_in_the_upstream_with_no_rows_inside_the_executor_waits_using_getsome_one_block) {
  // we are checking multiple input blocks
  // we are only fetching 1 row each (atMost = 1)
  // after a DONE is returned, it must stay done!

  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(blocka));
  blockDeque.push_back(std::move(blockb));
  blockDeque.push_back(std::move(blockc));
  blockDeque.push_back(std::move(blockd));
  blockDeque.push_back(std::move(blocke));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
  testee.addDependency(&dependency);
  size_t atMost = 1;
  size_t total = 0;

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);

  ASSERT_EQ(total, 5);
}

TEST_F(ExecutionBlockImplTest,
       there_are_multiple_blocks_in_the_upstream_with_no_rows_inside_the_executor_waits_using_getsome_multiple_blocks) {
  // as test above, BUT with a higher atMost value.

  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(blocka));
  blockDeque.push_back(std::move(blockb));
  blockDeque.push_back(std::move(blockc));
  blockDeque.push_back(std::move(blockd));
  blockDeque.push_back(std::move(blocke));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
  testee.addDependency(&dependency);
  size_t atMost = 2;
  size_t total = 0;

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  total = total + block->size();

  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(block, nullptr);

  ASSERT_EQ(total, 5);
}

TEST_F(ExecutionBlockImplTest,
       there_are_multiple_blocks_in_the_upstream_with_no_rows_inside_the_executor_waits_using_skipsome) {
  // we are checking multiple input blocks
  // we are only fetching 1 row each (atMost = 1)
  // after a DONE is returned, it must stay done!

  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(blocka));
  blockDeque.push_back(std::move(blockb));
  blockDeque.push_back(std::move(blockc));
  blockDeque.push_back(std::move(blockd));
  blockDeque.push_back(std::move(blocke));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
  testee.addDependency(&dependency);
  size_t atMost = 1;
  size_t skipped = 0;

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(skipped, 1);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(skipped, 1);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(skipped, 1);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(skipped, 1);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_EQ(skipped, 0);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(skipped, 1);

  std::tie(state, skipped) = testee.skipSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(skipped, 0);
}

TEST_F(ExecutionBlockImplTest,
       there_is_an_invalid_empty_block_in_the_upstream_the_executor_waits_using_getsome) {
  std::deque<SharedAqlItemBlockPtr> blockDeque;
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  blockDeque.push_back(std::move(block));

  WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

  ExecutionBlockImpl<TestEmptyExecutorHelper> testee(&engine, node, std::move(emptyInfos));
  testee.addDependency(&dependency);

  size_t atMost = 1000;
  std::tie(state, block) = testee.getSome(atMost);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(block, nullptr);
}

class SharedExecutionBlockImplTest {
 protected:
  mocks::MockAqlServer server{};
  ResourceMonitor monitor{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery{server.createFakeQuery()};
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

  SharedExecutionBlockImplTest() {
    auto engine =
        std::make_unique<ExecutionEngine>(*fakedQuery, SerializationFormat::SHADOWROWS);
    fakedQuery->setEngine(engine.release());
  }

  ExecutionNode* generateNodeDummy() {
    auto dummy = std::make_unique<SingletonNode>(fakedQuery->plan(), _execNodes.size());
    auto res = dummy.get();
    _execNodes.emplace_back(std::move(dummy));
    return res;
  }

  LambdaExecutorInfos makeInfos(ProduceCall call, RegisterId read = RegisterPlan::MaxRegisterId,
                                RegisterId write = RegisterPlan::MaxRegisterId) {
    if (read != RegisterPlan::MaxRegisterId) {
      EXPECT_LE(read, write);
    } else if (write != RegisterPlan::MaxRegisterId) {
      EXPECT_EQ(write, 0);
    }

    auto readAble = make_shared_unordered_set();
    auto writeAble = make_shared_unordered_set();
    auto registersToKeep = std::unordered_set<RegisterId>{};
    if (read != RegisterPlan::MaxRegisterId) {
      for (RegisterId i = 0; i <= read; ++i) {
        readAble->emplace(i);
        registersToKeep.emplace(i);
      }
      for (RegisterId i = read + 1; i <= write; ++i) {
        writeAble->emplace(i);
      }
    } else if (write != RegisterPlan::MaxRegisterId) {
      for (RegisterId i = 0; i <= write; ++i) {
        writeAble->emplace(i);
      }
    }
    RegisterId regsToRead = (read == RegisterPlan::MaxRegisterId) ? 0 : read + 1;
    RegisterId regsToWrite = (write == RegisterPlan::MaxRegisterId) ? 0 : write + 1;
    return LambdaExecutorInfos(readAble, writeAble, regsToRead, regsToWrite, {},
                               registersToKeep, std::move(call));
  }

  LambdaSkipExecutorInfos makeSkipInfos(ProduceCall call, SkipCall skipCall,
                                        RegisterId read = RegisterPlan::MaxRegisterId,
                                        RegisterId write = RegisterPlan::MaxRegisterId) {
    if (read != RegisterPlan::MaxRegisterId) {
      EXPECT_LE(read, write);
    } else if (write != RegisterPlan::MaxRegisterId) {
      EXPECT_EQ(write, 0);
    }

    auto readAble = make_shared_unordered_set();
    auto writeAble = make_shared_unordered_set();
    auto registersToKeep = std::unordered_set<RegisterId>{};
    if (read != RegisterPlan::MaxRegisterId) {
      for (RegisterId i = 0; i <= read; ++i) {
        readAble->emplace(i);
        registersToKeep.emplace(i);
      }
      for (RegisterId i = read + 1; i <= write; ++i) {
        writeAble->emplace(i);
      }
    } else if (write != RegisterPlan::MaxRegisterId) {
      for (RegisterId i = 0; i <= write; ++i) {
        writeAble->emplace(i);
      }
    }
    RegisterId regsToRead = (read == RegisterPlan::MaxRegisterId) ? 0 : read + 1;
    RegisterId regsToWrite = (write == RegisterPlan::MaxRegisterId) ? 0 : write + 1;
    return LambdaSkipExecutorInfos(readAble, writeAble, regsToRead, regsToWrite,
                                   {}, registersToKeep, std::move(call),
                                   std::move(skipCall));
  }

  std::unique_ptr<ExecutionBlock> createSingleton() {
    auto res = std::make_unique<ExecutionBlockImpl<IdExecutor<ConstFetcher>>>(
        fakedQuery->engine(), generateNodeDummy(), IdExecutorInfos{0, {}, {}});
    InputAqlItemRow inputRow{CreateInvalidInputRowHint{}};
    auto const [state, result] = res->initializeCursor(inputRow);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_TRUE(result.ok());
    return res;
  }

  ProduceCall generateProduceCall(size_t& nrCalls, AqlCall expectedCall,
                                  size_t numRowsLeftNoInput = ExecutionBlock::DefaultBatchSize,
                                  size_t numRowsLeftWithInput = ExecutionBlock::DefaultBatchSize) {
    return [&nrCalls, numRowsLeftNoInput, numRowsLeftWithInput,
            expectedCall](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
               -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      auto const& clientCall = output.getClientCall();
      if (nrCalls > 10) {
        EXPECT_TRUE(false);
        // This is emergency bailout, we ask way to often here
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      nrCalls++;
      if (input.hasDataRow()) {
        // We expact only the empty initial row, so just consume it
        auto const [state, row] = input.nextDataRow();
        EXPECT_EQ(state, ExecutorState::DONE);
        EXPECT_TRUE(row.isInitialized());
        EXPECT_EQ(output.numRowsLeft(), numRowsLeftWithInput);
      } else {
        EXPECT_EQ(output.numRowsLeft(), numRowsLeftNoInput);
      }
      EXPECT_EQ(clientCall.getOffset(), expectedCall.getOffset());
      EXPECT_EQ(clientCall.softLimit, expectedCall.softLimit);
      EXPECT_EQ(clientCall.hardLimit, expectedCall.hardLimit);
      EXPECT_EQ(clientCall.needsFullCount(), expectedCall.needsFullCount());

      NoStats stats{};
      AqlCall call{};
      return {input.upstreamState(), stats, call};
    };
  }

  SkipCall generateSkipCall(size_t& nrCalls, AqlCall expectedCall) {
    return [&nrCalls,
            expectedCall](AqlItemBlockInputRange& inputRange,
                          AqlCall& clientCall) -> std::tuple<ExecutorState, size_t, AqlCall> {
      if (nrCalls > 10) {
        EXPECT_TRUE(false);
        // This is emergency bailout, we ask way to often here
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      nrCalls++;
      EXPECT_EQ(clientCall.getOffset(), expectedCall.getOffset());
      EXPECT_EQ(clientCall.softLimit, expectedCall.softLimit);
      EXPECT_EQ(clientCall.hardLimit, expectedCall.hardLimit);
      EXPECT_EQ(clientCall.needsFullCount(), expectedCall.needsFullCount());
      size_t localSkip = 0;
      while (inputRange.hasDataRow() && clientCall.getOffset() > localSkip) {
        auto const& [state, input] = inputRange.nextDataRow();
        EXPECT_TRUE(input.isInitialized());
        localSkip++;
      }
      clientCall.didSkip(localSkip);

      AqlCall upstreamCall = clientCall;
      upstreamCall.softLimit = clientCall.getOffset() + clientCall.softLimit;
      upstreamCall.hardLimit = clientCall.getOffset() + clientCall.hardLimit;
      upstreamCall.offset = 0;

      return {inputRange.upstreamState(), localSkip, upstreamCall};
    };
  }

  SkipCall generateNeverSkipCall() {
    return [](AqlItemBlockInputRange& input,
              AqlCall& call) -> std::tuple<ExecutorState, size_t, AqlCall> {
      // Should not be called here. No Skip!
      EXPECT_TRUE(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    };
  }

  ProduceCall generateNeverProduceCall() {
    return [](AqlItemBlockInputRange& input,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      // Should not be called here. No limit, only skip!
      EXPECT_TRUE(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    };
  }
};

// Test of the execute() Logic stack
class ExecutionBlockImplExecuteSpecificTest : public SharedExecutionBlockImplTest,
                                              public testing::TestWithParam<bool> {
 protected:
  auto runTest(ProduceCall& prod, SkipCall& skip, AqlCall call)
      -> std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> {
    AqlCallStack stack{std::move(call)};
    auto singleton = createSingleton();
    if (GetParam()) {
      ExecutionBlockImpl<LambdaExePassThrough> testee{fakedQuery->engine(),
                                                      generateNodeDummy(),
                                                      makeInfos(prod)};
      testee.addDependency(singleton.get());
      return testee.execute(stack);
    } else {
      ExecutionBlockImpl<LambdaExe> testee{fakedQuery->engine(), generateNodeDummy(),
                                           makeSkipInfos(prod, skip)};
      testee.addDependency(singleton.get());
      return testee.execute(stack);
    }
  }
};

TEST_P(ExecutionBlockImplExecuteSpecificTest, test_toplevel_unlimited_call) {
  AqlCall fullCall{};
  size_t nrCalls = 0;

  ProduceCall execImpl = GetParam() ? generateProduceCall(nrCalls, fullCall, 0, 1)
                                    : generateProduceCall(nrCalls, fullCall);
  SkipCall skipCall = generateNeverSkipCall();
  auto [state, skipped, block] = runTest(execImpl, skipCall, fullCall);

  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(skipped, 0);
  EXPECT_EQ(block, nullptr);
  // Once with empty, once with the line by Singleton
  EXPECT_EQ(nrCalls, 2);
}

TEST_P(ExecutionBlockImplExecuteSpecificTest, test_toplevel_softlimit_call) {
  AqlCall fullCall{};
  fullCall.softLimit = 20;
  size_t nrCalls = 0;

  ProduceCall execImpl = GetParam() ? generateProduceCall(nrCalls, fullCall, 0, 1)
                                    : generateProduceCall(nrCalls, fullCall, 20, 20);
  SkipCall skipCall = generateNeverSkipCall();
  auto [state, skipped, block] = runTest(execImpl, skipCall, fullCall);

  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(skipped, 0);
  EXPECT_EQ(block, nullptr);
  // Once with empty, once with the line by Singleton
  EXPECT_EQ(nrCalls, 2);
}

TEST_P(ExecutionBlockImplExecuteSpecificTest, test_toplevel_hardlimit_call) {
  AqlCall fullCall{};
  fullCall.hardLimit = 20;
  size_t nrCalls = 0;

  ProduceCall execImpl = GetParam() ? generateProduceCall(nrCalls, fullCall, 0, 1)
                                    : generateProduceCall(nrCalls, fullCall, 20, 20);
  SkipCall skipCall = generateNeverSkipCall();
  auto [state, skipped, block] = runTest(execImpl, skipCall, fullCall);

  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(skipped, 0);
  EXPECT_EQ(block, nullptr);
  // Once with empty, once with the line by Singleton
  EXPECT_EQ(nrCalls, 2);
}

TEST_P(ExecutionBlockImplExecuteSpecificTest, test_toplevel_offset_call) {
  AqlCall fullCall{};
  fullCall.offset = 20;
  size_t nrCalls = 0;

  ProduceCall execImpl = generateNeverProduceCall();
  SkipCall skipCall = generateSkipCall(nrCalls, fullCall);

  auto [state, skipped, block] = runTest(execImpl, skipCall, fullCall);

  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(skipped, 1);
  if (GetParam()) {
    // Do never call skip, pass through
    EXPECT_EQ(nrCalls, 0);
  } else {
    // Call once without input, second with input
    EXPECT_EQ(nrCalls, 2);
  }

  EXPECT_EQ(block, nullptr);
}

TEST_P(ExecutionBlockImplExecuteSpecificTest, test_toplevel_offset_only_call) {
  AqlCall fullCall{};
  fullCall.offset = 20;
  // This test simulates a simple "skipSome" call on the old API.
  // It is releveant in any intermediate state.
  fullCall.softLimit = 0;
  size_t nrCalls = 0;

  ProduceCall execImpl = generateNeverProduceCall();
  SkipCall skipCall = generateSkipCall(nrCalls, fullCall);

  auto [state, skipped, block] = runTest(execImpl, skipCall, fullCall);

  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(skipped, 1);
  if (GetParam()) {
    // Do never call skip, pass through
    EXPECT_EQ(nrCalls, 0);
  } else {
    // Call once without input, second with input
    EXPECT_EQ(nrCalls, 2);
  }

  EXPECT_EQ(block, nullptr);
}

INSTANTIATE_TEST_CASE_P(ExecutionBlockImplExecuteTest,
                        ExecutionBlockImplExecuteSpecificTest, ::testing::Bool());

enum CallAsserterState { INITIAL, SKIP, GET, COUNT, DONE };

// Asserts if AqlCalls are forwarded onmodified correctly.
// TODO implement
struct SkipCallAsserter {
  explicit SkipCallAsserter(AqlCall const& expectedCall)
      : expected{expectedCall} {}

  size_t call = 0;
  size_t maxCall = 0;
  CallAsserterState state = CallAsserterState::INITIAL;
  AqlCall const expected;

  auto gotCalled(AqlCall const& got) -> void {
    call++;
    EXPECT_FALSE(got.getOffset() > 0);
    switch (state) {
      case CallAsserterState::INITIAL: {
        bool hasOffset = expected.getOffset() > 0;
        bool hasLimit = expected.getLimit() > 0;
        bool hasFullCount = expected.needsFullCount();
        if (hasOffset) {
          // TODO i think this is correct:
          // EXPECT_EQ(got.getOffset(), expected.getOffset());
          // This is how it is implemented right now
          EXPECT_EQ(got.getLimit(), expected.getOffset());
          state = CallAsserterState::SKIP;
        } else if (hasLimit) {
          EXPECT_EQ(got.getLimit(), expected.getLimit());
          state = CallAsserterState::GET;
        } else {
          // We do not test 0,0,false
          EXPECT_EQ(expected.needsFullCount(), true);
          // The executor does not need to care.
          EXPECT_EQ(got.needsFullCount(), expected.needsFullCount());
          state = CallAsserterState::COUNT;
        }
        maxCall = 1;
        if (hasOffset) {
          maxCall++;
        }
        if (hasLimit) {
          maxCall++;
        }
        if (hasFullCount) {
          maxCall++;
        }
        break;
      }
      case CallAsserterState::SKIP: {
        // TODO i think this is correct:
        // EXPECT_EQ(got.getOffset(), expected.getOffset());
        // This is how it is implemented right now
        EXPECT_EQ(got.getLimit(), expected.getOffset());
        if (expected.getLimit() > 0) {
          state = CallAsserterState::GET;
        } else if (expected.needsFullCount()) {
          state = CallAsserterState::COUNT;
        } else {
          state = CallAsserterState::DONE;
        }
        break;
      }
      case CallAsserterState::GET: {
        EXPECT_EQ(got.getOffset(), 0);
        EXPECT_EQ(got.getLimit(), expected.getLimit());
        if (expected.needsFullCount()) {
          state = CallAsserterState::COUNT;
        } else {
          state = CallAsserterState::DONE;
        }
        break;
      }
      case CallAsserterState::COUNT: {
        //  TODO Fixme, this needs improvement on Outputblock
        // EXPECT_EQ(got.getLimit(), 0);
        EXPECT_EQ(got.getLimit(), ExecutionBlock::DefaultBatchSize);
        EXPECT_EQ(got.getOffset(), 0);
        EXPECT_TRUE(got.needsFullCount());
        state = CallAsserterState::DONE;
        break;
      }
      case CallAsserterState::DONE: {
        // This should not be reached
        EXPECT_FALSE(true);
        break;
      }
    }
    EXPECT_LE(call, maxCall);
    if (call > maxCall) {
      LOG_DEVEL << got;
      if (call > maxCall + 2) {
        // Security bailout to avoid infinite loops
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
    }
  }
};

// Asserts if AqlCalls are forwarded onmodified correctly.
struct CallAsserter {
  explicit CallAsserter(AqlCall const& expectedCall) : expected{expectedCall} {}

  size_t call = 0;
  size_t maxCall = 0;
  CallAsserterState state = CallAsserterState::INITIAL;
  AqlCall const expected;

  auto gotCalled(AqlCall const& got) -> void {
    EXPECT_EQ(got.getOffset(), 0);
    call++;
    switch (state) {
      case CallAsserterState::INITIAL: {
        EXPECT_EQ(got.getLimit(), expected.getLimit());
        state = CallAsserterState::GET;
        maxCall = 1;
        if (expected.getLimit() > 0) {
          maxCall++;
        }
        break;
      }
      case CallAsserterState::GET: {
        EXPECT_EQ(got.getOffset(), 0);
        EXPECT_EQ(got.getLimit(), expected.getLimit());
        state = CallAsserterState::DONE;
        break;
      }
      case CallAsserterState::SKIP:
      case CallAsserterState::COUNT:
      case CallAsserterState::DONE: {
        // This should not be reached
        EXPECT_FALSE(true);
        break;
      }
    }
    EXPECT_LE(call, maxCall);
    if (call > maxCall) {
      // Security bailout to avoid infinite loops
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }
};

class ExecutionBlockImplExecuteIntegrationTest
    : public SharedExecutionBlockImplTest,
      public testing::TestWithParam<AqlCall> {
 protected:
  std::unique_ptr<ExecutionBlock> produceBlock(ExecutionBlock* dependency,
                                               std::shared_ptr<VPackBuilder> data,
                                               RegisterId outReg) {
    TRI_ASSERT(dependency != nullptr);
    TRI_ASSERT(data != nullptr);
    TRI_ASSERT(data->slice().isArray());
    // We make this a shared ptr just to make sure someone retains the data.
    auto iterator = std::make_shared<VPackArrayIterator>(data->slice());
    auto writeData = [data, outReg, iterator](AqlItemBlockInputRange& inputRange,
                                              OutputAqlItemRow& output)
        -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      while (inputRange.hasDataRow() && !output.isFull()) {
        auto const& [state, input] = inputRange.peekDataRow();
        EXPECT_TRUE(input.isInitialized());
        while (!output.isFull() && iterator->valid()) {
          output.cloneValueInto(outReg, input, AqlValue{iterator->value()});
          output.advanceRow();
          iterator->next();
        }
        if (!iterator->valid()) {
          // Consume input
          auto const& [state, input] = inputRange.nextDataRow();
          EXPECT_TRUE(input.isInitialized());
          iterator->reset();
        }
      }
      // We always use a default unlimited call here, we only have Singleton above.
      AqlCall call{};
      return {inputRange.upstreamState(), NoStats{}, call};
    };

    auto skipData =
        [data, iterator](AqlItemBlockInputRange& inputRange,
                         AqlCall& clientCall) -> std::tuple<ExecutorState, size_t, AqlCall> {
      size_t skipped = 0;
      while (inputRange.hasDataRow() &&
             (clientCall.getOffset() > 0 ||
              (clientCall.getLimit() == 0 && clientCall.needsFullCount()))) {
        auto const& [state, input] = inputRange.peekDataRow();
        EXPECT_TRUE(input.isInitialized());
        while ((clientCall.getOffset() > 0 ||
                (clientCall.getLimit() == 0 && clientCall.needsFullCount())) &&
               iterator->valid()) {
          clientCall.didSkip(1);
          skipped++;
          iterator->next();
        }
        if (!iterator->valid()) {
          // Consume input
          auto const& [state, input] = inputRange.nextDataRow();
          EXPECT_TRUE(input.isInitialized());
          iterator->reset();
        }
      }
      AqlCall call{};
      call.offset = 0;
      if (clientCall.getOffset() > 0) {
        call.softLimit = clientCall.getOffset();
      }  // else softLimit == unlimited
      call.fullCount = false;
      return {inputRange.upstreamState(), skipped, call};
    };
    auto infos = outReg == 0
                     ? makeSkipInfos(std::move(writeData), skipData,
                                     RegisterPlan::MaxRegisterId, outReg)
                     : makeSkipInfos(std::move(writeData), skipData, outReg - 1, outReg);
    auto producer =
        std::make_unique<ExecutionBlockImpl<LambdaExe>>(fakedQuery->engine(),
                                                        generateNodeDummy(),
                                                        std::move(infos));
    producer->addDependency(dependency);
    return producer;
  }

  std::unique_ptr<ExecutionBlock> forwardBlock(ExecutionBlock* dependency, RegisterId maxReg) {
    TRI_ASSERT(dependency != nullptr);
    auto forwardData = [](AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
        -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      while (inputRange.hasDataRow() && !output.isFull()) {
        auto const& [state, input] = inputRange.nextDataRow();
        EXPECT_TRUE(input.isInitialized());
        output.copyRow(input);
        output.advanceRow();
      }
      return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
    };
    auto producer = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
        fakedQuery->engine(), generateNodeDummy(),
        makeInfos(std::move(forwardData), maxReg, maxReg));
    producer->addDependency(dependency);
    return producer;
  }

  void ValidateResult(std::shared_ptr<VPackBuilder> data, size_t skipped,
                      SharedAqlItemBlockPtr result, RegisterId testReg) {
    auto const& call = GetParam();

    TRI_ASSERT(data != nullptr);
    TRI_ASSERT(data->slice().isArray());

    VPackSlice expected = data->slice();
    VPackArrayIterator expectedIt{expected};
    // Skip Part
    size_t offset =
        (std::min)(call.getOffset(), static_cast<size_t>(expected.length()));

    if (!call.needsFullCount()) {
      // Otherweise skipped = offset + fullCount
      EXPECT_EQ(offset, skipped);
    }

    for (size_t i = 0; i < offset; ++i) {
      // The first have been skipped
      expectedIt++;
    }
    size_t limit =
        (std::min)(call.getLimit(), static_cast<size_t>(expected.length()) - offset);
    if (result != nullptr) {
      // GetSome part
      EXPECT_EQ(limit, result->size());
      for (size_t i = 0; i < limit; ++i) {
        // The next have to match
        auto got = result->getValueReference(i, testReg).slice();
        EXPECT_TRUE(basics::VelocyPackHelper::equal(got, *expectedIt, false))
            << "Expected: " << expectedIt.value().toJson() << " got: " << got.toJson()
            << " in row " << i << " and register " << testReg;
        expectedIt++;
      }
    } else {
      EXPECT_EQ(limit, 0);
    }

    // Now test Fullcount
    if (call.needsFullCount()) {
      ASSERT_TRUE(expected.length() >= offset + limit);
      size_t fullCount = expected.length() - offset - limit;
      EXPECT_EQ(offset + fullCount, skipped);
    }
  }
};

TEST_P(ExecutionBlockImplExecuteIntegrationTest, test_produce_only) {
  auto singleton = createSingleton();

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  for (size_t i = 0; i < 1000; ++i) {
    builder->add(VPackValue(i));
  }
  builder->close();
  RegisterId outReg = 0;
  auto producer = produceBlock(singleton.get(), builder, outReg);

  auto const& call = GetParam();
  AqlCallStack stack{call};
  auto const [state, skipped, block] = producer->execute(stack);
  if (std::holds_alternative<size_t>(call.softLimit) && !call.hasHardLimit()) {
    EXPECT_EQ(state, ExecutionState::HASMORE);
  } else {
    EXPECT_EQ(state, ExecutionState::DONE);
  }

  ValidateResult(builder, skipped, block, outReg);
}

TEST_P(ExecutionBlockImplExecuteIntegrationTest, test_produce_using_two) {
  auto singleton = createSingleton();

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  for (size_t i = 0; i < 10; ++i) {
    builder->add(VPackValue(i));
  }
  builder->close();
  RegisterId outRegFirst = 0;
  RegisterId outRegSecond = 1;
  auto producerFirst = produceBlock(singleton.get(), builder, outRegFirst);
  auto producer = produceBlock(producerFirst.get(), builder, outRegSecond);
  auto const& call = GetParam();
  AqlCallStack stack{call};
  auto const [state, skipped, block] = producer->execute(stack);
  if (call.getLimit() < 100) {
    if (call.hasHardLimit()) {
      // On hard limit we need to stop
      EXPECT_EQ(state, ExecutionState::DONE);
    } else {
      // On soft limit we need to be able to produce more
      EXPECT_EQ(state, ExecutionState::HASMORE);
    }
  } else {
    EXPECT_FALSE(call.hasHardLimit());
    EXPECT_EQ(state, ExecutionState::DONE);
  }

  auto firstRegBuilder = std::make_shared<VPackBuilder>();
  auto secondRegBuilder = std::make_shared<VPackBuilder>();
  firstRegBuilder->openArray();
  secondRegBuilder->openArray();
  for (size_t i = 0; i < 10; ++i) {
    // i => 0 -> 9
    for (size_t j = 0; j < 10; ++j) {
      // j => 0 -> 9
      firstRegBuilder->add(VPackValue(i));
      secondRegBuilder->add(VPackValue(j));
    }
  }
  secondRegBuilder->close();
  firstRegBuilder->close();
  ValidateResult(firstRegBuilder, skipped, block, outRegFirst);
  ValidateResult(secondRegBuilder, skipped, block, outRegSecond);
}

// The following forwarding tests are disabled because we need to make modifications to the output row.

TEST_P(ExecutionBlockImplExecuteIntegrationTest, test_call_forwarding) {
  auto singleton = createSingleton();

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  for (size_t i = 0; i < 1000; ++i) {
    builder->add(VPackValue(i));
  }
  builder->close();
  RegisterId outReg = 0;
  auto producer = produceBlock(singleton.get(), builder, outReg);

  CallAsserter upperState{GetParam()};
  CallAsserter lowerState{GetParam()};

  auto testForwarding =
      [&](AqlItemBlockInputRange& inputRange,
          OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    upperState.gotCalled(output.getClientCall());
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto forwardCall = [&](AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    lowerState.gotCalled(output.getClientCall());
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto upper = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
      fakedQuery->engine(), generateNodeDummy(),
      makeInfos(std::move(testForwarding), outReg, outReg));
  upper->addDependency(producer.get());
  auto lower = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
      fakedQuery->engine(), generateNodeDummy(),
      makeInfos(std::move(forwardCall), outReg, outReg));
  lower->addDependency(upper.get());

  auto const& call = GetParam();
  AqlCallStack stack{call};
  auto const [state, skipped, block] = lower->execute(stack);
  if (std::holds_alternative<size_t>(call.softLimit) && !call.hasHardLimit()) {
    EXPECT_EQ(state, ExecutionState::HASMORE);
  } else {
    EXPECT_EQ(state, ExecutionState::DONE);
  }
  ValidateResult(builder, skipped, block, outReg);
}

TEST_P(ExecutionBlockImplExecuteIntegrationTest, test_call_forwarding_passthrough) {
  auto singleton = createSingleton();

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  for (size_t i = 0; i < 1000; ++i) {
    builder->add(VPackValue(i));
  }
  builder->close();
  RegisterId outReg = 0;
  auto producer = produceBlock(singleton.get(), builder, outReg);

  CallAsserter upperState{GetParam()};
  CallAsserter lowerState{GetParam()};

  auto testForwarding =
      [&](AqlItemBlockInputRange& inputRange,
          OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto forwardCall = [&](AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto upper = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
      fakedQuery->engine(), generateNodeDummy(),
      makeInfos(std::move(testForwarding), outReg, outReg));
  upper->addDependency(producer.get());
  auto lower = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
      fakedQuery->engine(), generateNodeDummy(),
      makeInfos(std::move(forwardCall), outReg, outReg));
  lower->addDependency(upper.get());

  auto const& call = GetParam();
  AqlCallStack stack{call};
  auto const [state, skipped, block] = lower->execute(stack);
  if (std::holds_alternative<size_t>(call.softLimit) && !call.hasHardLimit()) {
    EXPECT_EQ(state, ExecutionState::HASMORE);
  } else {
    EXPECT_EQ(state, ExecutionState::DONE);
  }
  ValidateResult(builder, skipped, block, outReg);
}

TEST_P(ExecutionBlockImplExecuteIntegrationTest, DISABLED_test_call_forwarding_implement_skip) {
  auto singleton = createSingleton();

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  for (size_t i = 0; i < 1000; ++i) {
    builder->add(VPackValue(i));
  }
  builder->close();
  RegisterId outReg = 0;
  auto producer = produceBlock(singleton.get(), builder, outReg);
  CallAsserter upperState{GetParam()};
  CallAsserter lowerState{GetParam()};

  auto testForwarding =
      [&](AqlItemBlockInputRange& inputRange,
          OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    upperState.gotCalled(output.getClientCall());
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto forwardCall = [&](AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
    lowerState.gotCalled(output.getClientCall());
    while (inputRange.hasDataRow() && !output.isFull()) {
      auto const& [state, input] = inputRange.nextDataRow();
      EXPECT_TRUE(input.isInitialized());
      output.copyRow(input);
      output.advanceRow();
    }
    return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
  };
  auto forwardSkipCall = [&](AqlItemBlockInputRange& inputRange,
                             AqlCall& call) -> std::tuple<ExecutorState, size_t, AqlCall> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  };

  auto upper = std::make_unique<ExecutionBlockImpl<LambdaExePassThrough>>(
      fakedQuery->engine(), generateNodeDummy(), makeInfos(std::move(testForwarding)));
  upper->addDependency(producer.get());
  auto lower = std::make_unique<ExecutionBlockImpl<TestLambdaSkipExecutor>>(
      fakedQuery->engine(), generateNodeDummy(),
      makeSkipInfos(std::move(forwardCall), std::move(forwardSkipCall)));
  lower->addDependency(upper.get());

  auto const& call = GetParam();
  AqlCallStack stack{call};
  auto const [state, skipped, block] = lower->execute(stack);
  if (std::holds_alternative<size_t>(call.softLimit) && !call.hasHardLimit()) {
    EXPECT_EQ(state, ExecutionState::HASMORE);
  } else {
    EXPECT_EQ(state, ExecutionState::DONE);
  }
  ValidateResult(builder, skipped, block, outReg);
}

// The numbers here are random, but all of them are below 1000 which is the default batch size
static constexpr auto defaultCall = []() -> const AqlCall { return AqlCall{}; };

static constexpr auto skipCall = []() -> const AqlCall {
  AqlCall res{};
  res.offset = 15;
  return res;
};

static constexpr auto softLimit = []() -> const AqlCall {
  AqlCall res{};
  res.softLimit = 35;
  return res;
};

static constexpr auto hardLimit = []() -> const AqlCall {
  AqlCall res{};
  res.hardLimit = 76;
  return res;
};

static constexpr auto fullCount = []() -> const AqlCall {
  AqlCall res{};
  res.hardLimit = 17;
  res.fullCount = true;
  return res;
};

static constexpr auto skipAndSoftLimit = []() -> const AqlCall {
  AqlCall res{};
  res.offset = 16;
  res.softLimit = 64;
  return res;
};

static constexpr auto skipAndHardLimit = []() -> const AqlCall {
  AqlCall res{};
  res.offset = 32;
  res.hardLimit = 71;
  return res;
};
static constexpr auto skipAndHardLimitAndFullCount = []() -> const AqlCall {
  AqlCall res{};
  res.offset = 8;
  res.hardLimit = 57;
  res.fullCount = true;
  return res;
};
static constexpr auto onlyFullCount = []() -> const AqlCall {
  AqlCall res{};
  res.hardLimit = 0;
  res.fullCount = true;
  return res;
};
static constexpr auto onlySkipAndCount = []() -> const AqlCall {
  AqlCall res{};
  res.offset = 16;
  res.hardLimit = 0;
  res.fullCount = true;
  return res;
};

INSTANTIATE_TEST_CASE_P(ExecutionBlockExecuteIntegration, ExecutionBlockImplExecuteIntegrationTest,
                        ::testing::Values(defaultCall(), skipCall(),
                                          softLimit(), hardLimit(), fullCount(),
                                          skipAndSoftLimit(), skipAndHardLimit(),
                                          skipAndHardLimitAndFullCount(),
                                          onlyFullCount(), onlySkipAndCount()));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
