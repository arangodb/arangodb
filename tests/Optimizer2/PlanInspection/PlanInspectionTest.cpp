////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Graph/GraphTestTools.h"  // tests/Graph
#include "Mocks/MockGraph.h"
#include "Mocks/MockGraphProvider.h"

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

// arangod/graph
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"

#include <unordered_set>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;
using namespace arangodb::graph;

namespace {
aql::AstNode* InitializeReference(aql::Ast& ast, aql::Variable& var) {
  ast.scopes()->start(aql::ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  aql::AstNode* a = ast.createNodeReference(var.name);
  ast.scopes()->endCurrent();
  return a;
}
}  // namespace

using Step = SingleServerProviderStep;

namespace arangodb {
namespace tests {
namespace plan_inspection_test {

class PlanInspectionTest : public ::testing::Test {
 protected:
  // Only used to mock a singleServer
  std::unique_ptr<GraphTestSetup> s{nullptr};
  std::unique_ptr<MockGraphDatabase> singleServer{nullptr};
  std::shared_ptr<arangodb::aql::Query> query{nullptr};
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};
  arangodb::aql::AqlFunctionsInternalCache _functionsCache{};
  std::unique_ptr<arangodb::aql::FixedVarExpressionContext> _expressionContext;
  std::unique_ptr<arangodb::transaction::Methods> _trx{};

  // Expression Parts
  aql::Variable* _tmpVar{nullptr};
  aql::AstNode* _varNode{nullptr};
  aql::Projections _vertexProjections{};
  aql::Projections _edgeProjections{};

  std::unordered_map<std::string, std::vector<std::string>> _emptyShardMap{};

  // can be used for further testing to generate a expression
  // std::string stringToMatch = "0-1";

  PlanInspectionTest() {}
  ~PlanInspectionTest() {}

  auto makeProvider(MockGraph const& graph)
      -> arangodb::graph::SingleServerProvider<SingleServerProviderStep> {
    // Setup code for each provider type
    s = std::make_unique<GraphTestSetup>();
    singleServer =
        std::make_unique<MockGraphDatabase>(s->server, "testVocbase");
    singleServer->addGraph(graph);

    // We now have collections "v" and "e"
    query = singleServer->getQuery("RETURN 1", {"v", "e"});
    _trx = std::make_unique<arangodb::transaction::Methods>(
        query->newTrxContext());

    auto edgeIndexHandle = singleServer->getEdgeIndexHandle("e");
    _tmpVar = singleServer->generateTempVar(query.get());

    auto indexCondition =
        singleServer->buildOutboundCondition(query.get(), _tmpVar);
    _varNode = ::InitializeReference(*query->ast(), *_tmpVar);

    std::vector<IndexAccessor> usedIndexes{};

    // can be used to create an expression, currently unused but may be helpful
    // for additional tests auto expr = conditionKeyMatches(stringToMatch);
    usedIndexes.emplace_back(IndexAccessor{edgeIndexHandle, indexCondition, 0,
                                           nullptr, std::nullopt, 0,
                                           TRI_EDGE_OUT});

    _expressionContext =
        std::make_unique<arangodb::aql::FixedVarExpressionContext>(
            *_trx, *query, _functionsCache);
    SingleServerBaseProviderOptions opts(
        _tmpVar,
        std::make_pair(
            std::move(usedIndexes),
            std::unordered_map<uint64_t, std::vector<IndexAccessor>>{}),
        *_expressionContext.get(), {}, _emptyShardMap, _vertexProjections,
        _edgeProjections, /*produceVertices*/ true);
    return {*query.get(), std::move(opts), _resourceMonitor};
  }

  /*
   * generates a condition #TMP._key == '<toMatch>'
   */
  std::unique_ptr<aql::Expression> conditionKeyMatches(
      std::string const& toMatch) {
    auto expectedKey =
        query->ast()->createNodeValueString(toMatch.c_str(), toMatch.length());
    auto keyAccess = query->ast()->createNodeAttributeAccess(
        _varNode, StaticStrings::KeyString);
    // This condition cannot be fulfilled
    auto condition = query->ast()->createNodeBinaryOperator(
        aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ, keyAccess, expectedKey);
    return std::make_unique<aql::Expression>(query->ast(), condition);
  }
};

TEST_F(PlanInspectionTest, create_plan_of_simple_query) {
  tests::graph::MockGraph g;  // note: Not needed itself, but ...
  makeProvider(g);  // ... this call will initialize everything we need
  // to actually generate our plan.

  std::shared_ptr<arangodb::aql::Query> myQuery =
      singleServer->getQuery("RETURN 1", {"v", "e"});
  auto actualQuery = myQuery.get();
  auto oldPlan = actualQuery->plan();

  arangodb::aql::optimizer2::plan::InspectablePlan newPlan =
      oldPlan->toInspectable();
  EXPECT_TRUE(newPlan.success());
  EXPECT_EQ(newPlan.amountOfNodes(), 3u);
}

}  // namespace plan_inspection_test
}  // namespace tests
}  // namespace arangodb