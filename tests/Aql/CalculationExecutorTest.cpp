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

#include "Aql/AqlCall.h"
#include "AqlExecutorTestCase.h"
#include "AqlItemBlockHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ResourceUsage.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// required for QuerySetup
#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
AstNode* initializeReference(Ast& ast, Variable& var) {
  ast.scopes()->start(ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  AstNode* a = ast.createNodeReference("a");
  ast.scopes()->endCurrent();
  return a;
}
}  // namespace

namespace arangodb {
namespace tests {
namespace aql {

using CalculationExecutorTestHelper = ExecutorTestHelper<2, 2>;
using CalculationExecutorSplitType = CalculationExecutorTestHelper::SplitType;
using CalculationExecutorInputParam = std::tuple<CalculationExecutorSplitType>;

// TODO Add tests for both
// CalculationExecutor<CalculationType::V8Condition> and
// CalculationExecutor<CalculationType::Reference>!

class CalculationExecutorTest
    : public AqlExecutorTestCaseWithParam<CalculationExecutorInputParam> {
 protected:
  ExecutionState state;
  AqlItemBlockManager itemBlockManager;
  Ast ast;
  AstNode* one;
  Variable var;
  AstNode* a;
  AstNode* node;
  ExecutionPlan plan;
  Expression expr;
  RegisterId outRegID;
  RegisterId inRegID;
  RegisterInfos registerInfos;
  CalculationExecutorInfos executorInfos;

  CalculationExecutorTest()
      : itemBlockManager(monitor, SerializationFormat::SHADOWROWS),
        ast(*fakedQuery.get()),
        one(ast.createNodeValueInt(1)),
        var("a", 0, false),
        a(::initializeReference(ast, var)),
        node(ast.createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_BINARY_PLUS, a, one)),
        plan(&ast, false),
        expr(&ast, node),
        outRegID(1),
        inRegID(0),
        registerInfos(RegIdSet{inRegID}, RegIdSet{outRegID},
                      RegisterId(1) /*in width*/, RegisterId(2) /*out width*/,
                      RegIdSet{} /*to clear*/, RegIdSetStack{{}} /*to keep*/),
        executorInfos(outRegID /*out reg*/, *fakedQuery.get() /*query*/, expr /*expression*/,
                      std::vector<Variable const*>{&var} /*expression input variables*/,
                      std::vector<RegisterId>({inRegID}) /*expression input registers*/) {}

  auto getSplit() -> CalculationExecutorSplitType {
    auto [split] = GetParam();
    return split;
  }

  auto buildInfos() -> CalculationExecutorInfos {
    return CalculationExecutorInfos{0, *fakedQuery.get(), expr, {&var}, {0}};
  }
};

template <size_t... vs>
const CalculationExecutorSplitType splitIntoBlocks =
    CalculationExecutorSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const CalculationExecutorSplitType splitStep = CalculationExecutorSplitType{step};

INSTANTIATE_TEST_CASE_P(CalculationExecutor, CalculationExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<1>, splitStep<2>));

TEST_P(CalculationExecutorTest, reference_empty_input) {
  AqlCall call{};
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Reference>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue({})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(CalculationExecutorTest, reference_some_input) {
  AqlCall call{};
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Reference>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{0, 0}, RowBuilder<2>{1, 1},
                                             RowBuilder<2>{R"("a")", R"("a")"},
                                             RowBuilder<2>{2, 2}, RowBuilder<2>{3, 3},
                                             RowBuilder<2>{4, 4}, RowBuilder<2>{5, 5},
                                             RowBuilder<2>{6, 6}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, reference_some_input_skip) {
  AqlCall call{};
  call.offset = 4;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Reference>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{3, 3}, RowBuilder<2>{4, 4},
                                             RowBuilder<2>{5, 5}, RowBuilder<2>{6, 6}})
      .allowAnyOutputOrder(false)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, reference_some_input_limit) {
  AqlCall call{};
  call.hardLimit = 4u;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Reference>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{0, 0}, RowBuilder<2>{1, 1},
                                             RowBuilder<2>{R"("a")", R"("a")"},
                                             RowBuilder<2>{2, 2}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, reference_some_input_limit_fullcount) {
  AqlCall call{};
  call.hardLimit = 4u;
  call.fullCount = true;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Reference>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{0, 0}, RowBuilder<2>{1, 1},
                                             RowBuilder<2>{R"("a")", R"("a")"},
                                             RowBuilder<2>{2, 2}})
      .allowAnyOutputOrder(false)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, condition_some_input) {
  AqlCall call{};
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Condition>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1},
                    MatrixBuilder<2>{RowBuilder<2>{0, 1}, RowBuilder<2>{1, 2},
                                     RowBuilder<2>{R"("a")", 1}, RowBuilder<2>{2, 3},
                                     RowBuilder<2>{3, 4}, RowBuilder<2>{4, 5},
                                     RowBuilder<2>{5, 6}, RowBuilder<2>{6, 7}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, condition_some_input_skip) {
  AqlCall call{};
  call.offset = 4;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Condition>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, MatrixBuilder<2>{RowBuilder<2>{3, 4}, RowBuilder<2>{4, 5},
                                             RowBuilder<2>{5, 6}, RowBuilder<2>{6, 7}})
      .allowAnyOutputOrder(false)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, condition_some_input_limit) {
  AqlCall call{};
  call.hardLimit = 4u;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Condition>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1},
                    MatrixBuilder<2>{RowBuilder<2>{0, 1}, RowBuilder<2>{1, 2},
                                     RowBuilder<2>{R"("a")", 1}, RowBuilder<2>{2, 3}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

TEST_P(CalculationExecutorTest, condition_some_input_limit_fullcount) {
  AqlCall call{};
  call.hardLimit = 4u;
  call.fullCount = true;
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::Condition>>(std::move(registerInfos),
                                                                    std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1},
                    MatrixBuilder<2>{RowBuilder<2>{0, 1}, RowBuilder<2>{1, 2},
                                     RowBuilder<2>{R"("a")", 1}, RowBuilder<2>{2, 3}})
      .allowAnyOutputOrder(false)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

// Could be fixed and enabled if one enabled the V8 engine
TEST_P(CalculationExecutorTest, DISABLED_v8condition_some_input) {
  AqlCall call{};
  ExecutionStats stats{};

  makeExecutorTestHelper<2, 2>()
      .addConsumer<CalculationExecutor<CalculationType::V8Condition>>(std::move(registerInfos),
                                                                      std::move(executorInfos))
      .setInputValue(MatrixBuilder<2>{
          RowBuilder<2>{0, NoneEntry{}}, RowBuilder<2>{1, NoneEntry{}},
          RowBuilder<2>{R"("a")", NoneEntry{}}, RowBuilder<2>{2, NoneEntry{}},
          RowBuilder<2>{3, NoneEntry{}}, RowBuilder<2>{4, NoneEntry{}},
          RowBuilder<2>{5, NoneEntry{}}, RowBuilder<2>{6, NoneEntry{}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1},
                    MatrixBuilder<2>{RowBuilder<2>{0, 1}, RowBuilder<2>{1, 2},
                                     RowBuilder<2>{R"("a")", 1}, RowBuilder<2>{2, 3},
                                     RowBuilder<2>{3, 4}, RowBuilder<2>{4, 5},
                                     RowBuilder<2>{5, 6}, RowBuilder<2>{6, 7}})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(true);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
