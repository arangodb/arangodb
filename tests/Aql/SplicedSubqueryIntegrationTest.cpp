////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
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
#include "Aql/SubqueryEndExecutor.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "Aql/TestLambdaExecutor.h"
#include "Aql/WaitingExecutionBlockMock.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

using RegisterSet = std::unordered_set<RegisterId>;
using LambdaExePassThrough = TestLambdaExecutor;
using LambdaExe = TestLambdaSkipExecutor;

class SplicedSubqueryIntegrationTest : public ::testing::Test {
 protected:
  SharedAqlItemBlockPtr result;

  fakeit::Mock<ExecutionEngine> mockEngine;
  ExecutionEngine& engine;

  fakeit::Mock<AqlItemBlockManager> mockBlockManager;
  AqlItemBlockManager& itemBlockManager;

  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx;

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& context;

  fakeit::Mock<Query> mockQuery;
  Query& query;

  ExecutionState state;
  ResourceMonitor monitor;

  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions;
  ProfileLevel profile;

  // This is not used thus far in Base-Clase
  ExecutionNode const* node = nullptr;
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

  SharedAqlItemBlockPtr block;

 public:
  SplicedSubqueryIntegrationTest()
      : engine(mockEngine.get()),
        itemBlockManager(mockBlockManager.get()),
        trx(mockTrx.get()),
        context(mockContext.get()),
        query(mockQuery.get()),
        lqueryOptions(mockQueryOptions.get()),
        profile(ProfileLevel(PROFILE_LEVEL_NONE)),
        node(nullptr),
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

 protected:
  auto generateNodeDummy() -> ExecutionNode* {
    auto dummy = std::make_unique<SingletonNode>(query.plan(), _execNodes.size());
    auto res = dummy.get();
    _execNodes.emplace_back(std::move(dummy));
    return res;
  }

  auto createSingleton() -> std::unique_ptr<ExecutionBlock> {
    std::deque<SharedAqlItemBlockPtr> blockDeque;
    SharedAqlItemBlockPtr block = buildBlock<0>(itemBlockManager, {{}});
    blockDeque.push_back(std::move(block));
    return std::make_unique<WaitingExecutionBlockMock>(
        &engine, generateNodeDummy(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  }

  auto createSubqueryStartExecutor(RegisterId numRegs)
      -> ExecutionBlockImpl<SubqueryStartExecutor> {
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    std::unordered_set<RegisterId> toKeep;
    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = SubqueryStartExecutor::Infos(emptyRegisterList, emptyRegisterList,
                                              numRegs, numRegs, {}, toKeep);

    return ExecutionBlockImpl<SubqueryStartExecutor>{&engine, node, std::move(infos)};
  }

  auto createSubqueryEndExecutor(RegisterId numRegs)
      -> ExecutionBlockImpl<SubqueryEndExecutor> {
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = SubqueryEndExecutor::Infos(emptyRegisterList, emptyRegisterList,
                                            numRegs, numRegs, {}, toKeep, nullptr,
                                            RegisterId{0}, RegisterId{1});

    return ExecutionBlockImpl<SubqueryEndExecutor>{&engine, node, std::move(infos)};
  }

  auto createProduceCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      if (input.hasDataRow()) {
        auto const [state, row] = input.nextDataRow();
      }
      NoStats stats{};
      AqlCall call{};

      return {input.upstreamState(), stats, call};
    };
  }

  auto createSkipCall() -> SkipCall {
    return [](AqlItemBlockInputRange& input,
              AqlCall& call) -> std::tuple<ExecutorState, size_t, AqlCall> {
      auto skipped = size_t{0};
      while (input.hasDataRow() && call.shouldSkip()) {
        auto const& [state, inputRow] = input.nextDataRow();
        EXPECT_TRUE(inputRow.isInitialized());
        skipped++;
      }
      call.didSkip(skipped);
      auto upstreamCall = AqlCall{call};
      return {input.upstreamState(), skipped, upstreamCall};
    };
  }

  auto createDoNothingExecutor() -> ExecutionBlockImpl<LambdaExe> {
    auto numRegs = size_t{1};
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = LambdaExe::Infos(emptyRegisterList, emptyRegisterList, numRegs, numRegs,
                                  {}, toKeep, createProduceCall(), createSkipCall());

    return ExecutionBlockImpl<LambdaExe>{&engine, node, std::move(infos)};
  }
};

TEST_F(SplicedSubqueryIntegrationTest, check_properties) { EXPECT_TRUE(true); };

TEST_F(SplicedSubqueryIntegrationTest, check_shadow_row_handling) {
  auto stack = AqlCallStack{AqlCall{}};

  auto singleton = createSingleton();
  auto startExec = createSubqueryStartExecutor(1);
  auto endExec = createSubqueryEndExecutor(1);

  startExec.addDependency(singleton.get());
  endExec.addDependency(&startExec);

  endExec.execute(stack);
};

TEST_F(SplicedSubqueryIntegrationTest, check_shadow_row_handling_2) {
  auto startExec = createSubqueryStartExecutor(2);
  auto middleExec = createDoNothingExecutor();
  auto endExec = createSubqueryEndExecutor(2);

  middleExec.addDependency(&startExec);
  endExec.addDependency(&middleExec);
};
