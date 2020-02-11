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

  using RegisterSet = std::unordered_set<RegisterId>;
  using SharedRegisterSet = std::shared_ptr<RegisterSet>;

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
    block = buildBlock<2>(itemBlockManager, {{{{R"("v")"}, {0}}},
                                             {{{R"("x")"}, {1}}},
                                             {{{R"("y")"}, {2}}},
                                             {{{R"("z")"}, {3}}},
                                             {{{R"("a")"}, {4}}},
                                             {{{R"("b")"}, {5}}}});
    blockDeque.push_back(std::move(block));
    return std::make_unique<WaitingExecutionBlockMock>(
        &engine, generateNodeDummy(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  }

  auto createSubqueryStartExecutor() -> ExecutionBlockImpl<SubqueryStartExecutor> {
    // Subquery start executor does not care about input or output registers?
    // TODO: talk about registers & register planning

    auto inputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{0});
    auto outputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{});
    auto toKeepRegisterSet = RegisterSet{0};

    auto infos = SubqueryStartExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                              inputRegisterSet->size(),
                                              inputRegisterSet->size() +
                                                  outputRegisterSet->size(),
                                              {}, toKeepRegisterSet);
    return ExecutionBlockImpl<SubqueryStartExecutor>{&engine, node, std::move(infos)};
  }

  // Subquery end executor has an input and an output register,
  // but only the output register is used, remove input reg?
  auto createSubqueryEndExecutor() -> ExecutionBlockImpl<SubqueryEndExecutor> {
    auto const inputRegister = RegisterId{0};
    auto const outputRegister = RegisterId{1};
    auto inputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{inputRegister});
    auto outputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{outputRegister});
    auto toKeepRegisterSet = RegisterSet{0};

    auto infos =
        SubqueryEndExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                   inputRegisterSet->size(),
                                   inputRegisterSet->size() + outputRegisterSet->size(),
                                   {}, toKeepRegisterSet, nullptr,
                                   inputRegister, outputRegister);

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
  auto startExec = createSubqueryStartExecutor();
  auto endExec = createSubqueryEndExecutor();

  startExec.addDependency(singleton.get());
  endExec.addDependency(&startExec);

  endExec.execute(stack);
};

TEST_F(SplicedSubqueryIntegrationTest, check_shadow_row_handling_2) {
  auto stack = AqlCallStack{AqlCall{}};

  auto singleton = createSingleton();
  auto startExecOuter = createSubqueryStartExecutor();
  auto endExecOuter = createSubqueryEndExecutor();

  auto startExecInner = createSubqueryStartExecutor();
  auto endExecInner = createSubqueryEndExecutor();

  startExecOuter.addDependency(singleton.get());
  startExecInner.addDependency(&startExecOuter);
  endExecInner.addDependency(&startExecInner);
  endExecOuter.addDependency(&endExecInner);

  auto const [state, num, block] = endExecOuter.execute(stack);

  LOG_DEVEL << num << " " << block->size() << " " << block->getNrRegs();
  for (auto i = size_t{0}; i < block->size(); i++) {
    LOG_DEVEL << " " << block->getValue(i, RegisterId{0}).slice().toJson()
              << " " << block->getValue(i, RegisterId{1}).slice().toJson();
  }
};
