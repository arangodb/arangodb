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
#include "Aql/Ast.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// required for QuerySetup
#include "../Mocks/Servers.h"

// Query
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"

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

// TODO Add tests for both
// CalculationExecutor<CalculationType::V8Condition> and
// CalculationExecutor<CalculationType::Reference>!

class CalculationExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
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
      : itemBlockManager(&monitor),
        fakedQuery(server.createFakeQuery()),
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
};

TEST_F(CalculationExecutorTest, there_are_no_rows_upstream_the_producer_does_not_wait) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  VPackBuilder input;
  SingleRowFetcherHelper<true> fetcher(input.steal(), false);
  CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(CalculationExecutorTest, there_are_no_rows_upstream_the_producer_waits) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  VPackBuilder input;
  SingleRowFetcherHelper<true> fetcher(input.steal(), true);
  CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateListExecutor::produceRows().
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(CalculationExecutorTest, there_are_rows_in_the_upstream_the_producer_does_not_wait) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  auto input = VPackParser::fromJson("[ [0], [1], [2] ]");
  SingleRowFetcherHelper<true> fetcher(input->steal(), false);
  CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
  NoStats stats{};

  OutputAqlItemRow row{std::move(block), infos.getOutputRegisters(),
                       infos.registersToKeep(), infos.registersToClear()};

  // 1
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  // 2
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  // 3
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!row.produced());

  // verify calculation
  {
    AqlValue value;
    auto block = row.stealBlock();
    for (std::size_t index = 0; index < 3; index++) {
      value = block->getValue(index, outRegID);
      ASSERT_TRUE(value.isNumber());
      ASSERT_TRUE(value.toInt64() == static_cast<int64_t>(index + 1));
    }
  }
}

TEST_F(CalculationExecutorTest, there_are_rows_in_the_upstream_the_producer_waits) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
  auto input = VPackParser::fromJson("[ [0], [1], [2] ]");
  SingleRowFetcherHelper<true> fetcher(input->steal(), true);
  CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
  NoStats stats{};

  OutputAqlItemRow row{std::move(block), infos.getOutputRegisters(),
                       infos.registersToKeep(), infos.registersToClear()};

  // waiting
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  // 1
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  // waiting
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  // 2
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  // waiting
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!row.produced());

  // 3
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
