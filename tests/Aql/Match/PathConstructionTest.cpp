////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Match/PathConstruction.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Mocks/Servers.h"

using namespace arangodb::aql;
using namespace arangodb::aql::match;

namespace {

class PathConstructionTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  std::shared_ptr<Query> query{server.createFakeQuery()};
  Ast ast{*query};
  ExecutionPlan plan{&ast, false};
  PathConstruction paths{plan, &ast};
};

}  // namespace

TEST_F(PathConstructionTest, addPathVertexAndEdgeAppendReferences) {
  Variable const* v = ast.variables()->createTemporaryVariable();
  Variable const* e = ast.variables()->createTemporaryVariable();
  std::vector<AstNode const*> vertices;
  std::vector<AstNode const*> edges;
  paths.addPathVertex(vertices, v);
  paths.addPathEdge(edges, e);
  ASSERT_EQ(1U, vertices.size());
  ASSERT_EQ(1U, edges.size());
  EXPECT_EQ(NODE_TYPE_REFERENCE, vertices.front()->type);
  EXPECT_EQ(NODE_TYPE_REFERENCE, edges.front()->type);
}

TEST_F(PathConstructionTest, appendTraversalPathSplicesVerticesAndEdges) {
  Variable const* start = ast.variables()->createTemporaryVariable();
  Variable const* path = ast.variables()->createTemporaryVariable();
  std::vector<AstNode const*> vertices;
  std::vector<AstNode const*> edges;
  paths.addPathVertex(vertices, start);
  paths.appendTraversalPath(vertices, edges, path);

  ASSERT_EQ(1U, vertices.size());
  ASSERT_EQ(1U, edges.size());
  EXPECT_EQ(NODE_TYPE_ARRAY_SPLICE, vertices.front()->type);
  EXPECT_EQ(NODE_TYPE_ARRAY_SPLICE, edges.front()->type);
}

TEST_F(PathConstructionTest, constructPathObjectHasEdgesAndVertices) {
  Variable const* out = ast.variables()->createTemporaryVariable();
  Variable const* v = ast.variables()->createTemporaryVariable();
  Variable const* e = ast.variables()->createTemporaryVariable();
  std::vector<AstNode const*> vertices;
  std::vector<AstNode const*> edges;
  paths.addPathVertex(vertices, v);
  paths.addPathEdge(edges, e);

  CalculationNode* calc = paths.constructPathObject(out, vertices, edges);
  ASSERT_NE(nullptr, calc);
  AstNode const* root = calc->expression()->node();
  ASSERT_EQ(NODE_TYPE_OBJECT, root->type);
  ASSERT_EQ(2U, root->numMembers());
  EXPECT_EQ("edges", root->getMember(0)->getStringView());
  EXPECT_EQ("vertices", root->getMember(1)->getStringView());
}

TEST_F(PathConstructionTest, finalizePatternWithoutPathReturnsPrevious) {
  auto* singleton = plan.createNode<SingletonNode>(&plan, plan.nextId());
  auto* result = paths.finalizePattern(singleton, {}, nullptr, {}, {});
  EXPECT_EQ(singleton, result);
}

TEST_F(PathConstructionTest, finalizePatternWithPathAddsCalculation) {
  auto* singleton = plan.createNode<SingletonNode>(&plan, plan.nextId());
  Variable const* path = ast.variables()->createTemporaryVariable();
  Variable const* v = ast.variables()->createTemporaryVariable();
  std::vector<AstNode const*> vertices;
  paths.addPathVertex(vertices, v);

  auto* result = paths.finalizePattern(singleton, {}, path, vertices, {});
  ASSERT_NE(nullptr, result);
  EXPECT_EQ(ExecutionNode::CALCULATION, result->getType());
  EXPECT_EQ(singleton, result->getFirstDependency());
}
