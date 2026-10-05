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

#include "Aql/Match/MatchTestHelper.h"

#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/TypedAstNodes.h"
#include "Containers/SmallVector.h"
#include "Graph/TraverserOptions.h"

using namespace arangodb::aql;
using namespace arangodb::aql::match;
using namespace arangodb::tests::aql::match;

namespace {

class BuilderTest : public MatchTestFixture {};

TraversalNode* firstTraversal(ExecutionPlan const& plan) {
  arangodb::containers::SmallVector<ExecutionNode*, 8> nodes;
  plan.findNodesOfType(nodes, ExecutionNode::TRAVERSAL, true);
  if (nodes.empty()) {
    return nullptr;
  }
  return ExecutionNode::castTo<TraversalNode*>(nodes.front());
}

}  // namespace

TEST_F(BuilderTest, simpleVertexEnumeratesCollection) {
  auto parsed = parseMatch("MATCH (v :vc) RETURN v");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::ENUMERATE_COLLECTION), 1U);
  EXPECT_EQ(0U, countNodesOfType(*plan, ExecutionNode::TRAVERSAL));
}

TEST_F(BuilderTest, fixedLengthSingleCollectionUsesEnumerateNotTraversal) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN [v, e, w]");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::ENUMERATE_COLLECTION), 2U);
  EXPECT_EQ(0U, countNodesOfType(*plan, ExecutionNode::TRAVERSAL));
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::FILTER), 1U);
}

TEST_F(BuilderTest, fixedLengthMultiCollectionUsesTraversal) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc) RETURN [v, e, w]");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::TRAVERSAL), 1U);
}

TEST_F(BuilderTest, variableLengthBoundedRangeUsesTraversal) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 1..3 ]-> (w :vc) RETURN [v, e, w]");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  auto* traversal = firstTraversal(*plan);
  ASSERT_NE(nullptr, traversal);
  ASSERT_NE(nullptr, traversal->options());
  EXPECT_EQ(1U, traversal->options()->minDepth);
  EXPECT_EQ(3U, traversal->options()->maxDepth);
}

TEST_F(BuilderTest, explicitFixedRangeUsesTraversalNotDefaultOneHop) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 2..2 ]-> (w :vc) RETURN [v, e, w]");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  auto* traversal = firstTraversal(*plan);
  ASSERT_NE(nullptr, traversal);
  ASSERT_NE(nullptr, traversal->options());
  EXPECT_EQ(2U, traversal->options()->minDepth);
  EXPECT_EQ(2U, traversal->options()->maxDepth);
}

TEST_F(BuilderTest, pathVariableAddsCalculation) {
  auto parsed = parseMatch("MATCH p = (v :vc) -[ e :ec ]-> (w :vc) RETURN p");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::CALCULATION), 1U);
}

TEST_F(BuilderTest, laterCommaPatternReadsFullDocumentNotProjection) {
  auto parsed = parseMatch(
      "MATCH (u :vc {_key: 'u1'} RETURN name), "
      "(w :vc WHERE w.age == u.age) RETURN w");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);

  arangodb::containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  bool sawAgeComparison = false;
  bool sawProjectionOfU = false;
  for (ExecutionNode* node : nodes) {
    auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
    if (calc->outVariable() != nullptr && calc->outVariable()->name == "u") {
      sawProjectionOfU = true;
    }
    AstNode const* expr = calc->expression()->node();
    if (expr == nullptr || expr->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
      continue;
    }
    ast::RelationalOperatorNode eq(expr);
    AstNode const* left = eq.getLeft();
    AstNode const* right = eq.getRight();
    if (left->type != NODE_TYPE_ATTRIBUTE_ACCESS ||
        right->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
      continue;
    }
    ast::AttributeAccessNode leftAccess(left);
    ast::AttributeAccessNode rightAccess(right);
    if (leftAccess.getAttributeName() != "age" ||
        rightAccess.getAttributeName() != "age") {
      continue;
    }
    ASSERT_EQ(NODE_TYPE_REFERENCE, leftAccess.getObject()->type);
    ASSERT_EQ(NODE_TYPE_REFERENCE, rightAccess.getObject()->type);
    auto leftName =
        ast::ReferenceNode(leftAccess.getObject()).getVariable()->name;
    auto rightName =
        ast::ReferenceNode(rightAccess.getObject()).getVariable()->name;
    EXPECT_EQ("w", leftName);
    EXPECT_NE("u", rightName);
    sawAgeComparison = true;
  }

  EXPECT_TRUE(sawProjectionOfU);
  EXPECT_TRUE(sawAgeComparison);
}

TEST_F(BuilderTest, inPatternProjectionAddsCalculation) {
  auto parsed =
      parseMatch("MATCH (v :vc RETURN name) -[ e :ec ]-> (w :vc) RETURN v");
  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
  EXPECT_GE(countNodesOfType(*plan, ExecutionNode::CALCULATION), 1U);
}
