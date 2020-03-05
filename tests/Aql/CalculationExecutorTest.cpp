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
#include "AqlItemBlockHelper.h"
#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
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
  CalculationExecutorInfos infos;

  CalculationExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        ast(fakedQuery.get()),
        one(ast.createNodeValueInt(1)),
        var("a", 0),
        a(::initializeReference(ast, var)),
        node(ast.createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_BINARY_PLUS, a, one)),
        plan(&ast),
        expr(&plan, &ast, node),
        outRegID(1),
        inRegID(0),
        infos(outRegID /*out reg*/, RegisterId(1) /*in width*/, RegisterId(2) /*out width*/,
              std::unordered_set<RegisterId>{} /*to clear*/,
              std::unordered_set<RegisterId>{} /*to keep*/,
              *fakedQuery.get() /*query*/, expr /*expression*/,
              std::vector<Variable const*>{&var} /*expression in variables*/,
              std::vector<RegisterId>{inRegID} /*expression in registers*/) {}

  auto getSplit() -> CalculationExecutorSplitType {
    auto [split] = GetParam();
    return split;
  }

  auto buildInfos() -> CalculationExecutorInfos {
    return CalculationExecutorInfos{0,    1,      1,  {}, {}, *fakedQuery.get(),
                                    expr, {&var}, {0}};
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

TEST_P(CalculationExecutorTest, empty_input) {
  auto infos = buildInfos();
  AqlCall call{};
  ExecutionStats stats{};

  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<CalculationExecutor<CalculationType::Condition>>(std::move(infos))
      .setInputValue({})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(CalculationExecutorTest, some_input) {
  auto infos = buildInfos();
  AqlCall call{};
  ExecutionStats stats{};

  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<CalculationExecutor<CalculationType::Condition>>(std::move(infos))
      .setInputValueList(0, 1, 2, 3, 4, 5, 6, 7, 8)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
