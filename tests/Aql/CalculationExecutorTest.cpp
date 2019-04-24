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

#include "BlockFetcherHelper.h"
#include "catch.hpp"
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

namespace arangodb {
namespace tests {
namespace aql {

// TODO Add tests for both
// CalculationExecutor<CalculationType::V8Condition> and
// CalculationExecutor<CalculationType::Reference>!

SCENARIO("CalculationExecutor", "[AQL][EXECUTOR][CALC]") {
  ExecutionState state;

  // create block manager
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  //// create query and expression
  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();

  Ast ast{fakedQuery.get()};
  // build expression to evaluate
  AstNode* one = ast.createNodeValueInt(1);
  Variable var{"a", 0};
  ast.scopes()->start(ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  AstNode* a = ast.createNodeReference("a");
  ast.scopes()->endCurrent();
  AstNode* node =
      ast.createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_BINARY_PLUS, a, one);

  ExecutionPlan plan{&ast};
  Expression expr(&plan, &ast, node);

  auto outRegID = RegisterId(1);
  auto inRegID = RegisterId(0);

  CalculationExecutorInfos infos(outRegID /*out reg*/, RegisterId(1) /*in width*/,
                                 RegisterId(2) /*out width*/,
                                 std::unordered_set<RegisterId>{} /*to clear*/,
                                 std::unordered_set<RegisterId>{} /*to keep*/,
                                 *fakedQuery.get() /*query*/, expr /*expression*/,
                                 std::vector<Variable const*>{&var} /*expression in variables*/,
                                 std::vector<RegisterId>{inRegID} /*expression in registers*/
  );

  GIVEN("there are no rows upstream") {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<true> fetcher(input.steal(), false);
      CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<true> fetcher(input.steal(), true);
      CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }

  }  // GIVEN

  GIVEN("there are rows in the upstream") {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};

    auto input = VPackParser::fromJson("[ [0], [1], [2] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<true> fetcher(input->steal(), false);
      CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row{std::move(block), infos.getOutputRegisters(),
                             infos.registersToKeep(), infos.registersToClear()};

        // 1
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // 2
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // 3
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }

        // verify calculation
        AqlValue value;
        auto block = row.stealBlock();
        for (std::size_t index = 0; index < 3; index++) {
          value = block->getValue(index, outRegID);
          REQUIRE(value.isNumber());
          REQUIRE(value.toInt64() == index + 1);
        }

      }  // THEN
    }    // WHEN

    WHEN("the producer waits") {
      SingleRowFetcherHelper<true> fetcher(input->steal(), true);
      CalculationExecutor<CalculationType::Condition> testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row{std::move(block), infos.getOutputRegisters(),
                             infos.registersToKeep(), infos.registersToClear()};

        // waiting
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 1
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // waiting
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 2
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // waiting
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 3
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }
  }

}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
