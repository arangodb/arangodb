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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer/Rule/RemoveFiltersCoveredByTraversal.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Mocks/Servers.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

namespace arangodb::aql {
namespace {

class ConditionRemoveForTraversalTest : public ::testing::Test {
 protected:
  tests::mocks::MockAqlServer _server;
  std::shared_ptr<Query> _query{_server.createFakeQuery()};
  ExecutionPlan* _plan{const_cast<ExecutionPlan*>(_query->plan())};
  Ast* _ast{_plan->getAst()};
  Variable* _v{_ast->variables()->createTemporaryVariable()};
  Variable* _e{_ast->variables()->createTemporaryVariable()};
  Variable* _p{_ast->variables()->createTemporaryVariable()};

  AstNode* cmpInt(AstNodeType op, Variable const* v, char const* attr,
                  int64_t value) {
    AstNode* ref = _ast->createNodeReference(v);
    AstNode* access = _ast->createNodeAttributeAccess(ref, attr);
    AstNode* val = _ast->createNodeValueInt(value);
    return _ast->createNodeBinaryOperator(op, access, val);
  }

  AstNode* cmpAttr(AstNodeType op, Variable const* lhsVar, char const* lhsAttr,
                   Variable const* rhsVar, char const* rhsAttr) {
    AstNode* lhs = _ast->createNodeAttributeAccess(
        _ast->createNodeReference(lhsVar), lhsAttr);
    AstNode* rhs = _ast->createNodeAttributeAccess(
        _ast->createNodeReference(rhsVar), rhsAttr);
    return _ast->createNodeBinaryOperator(op, lhs, rhs);
  }

  AstNode* pathArrayEq(Variable const* pathVar, int64_t value) {
    AstNode* vertices = _ast->createNodeAttributeAccess(
        _ast->createNodeReference(pathVar), "vertices");

    _ast->scopes()->start(AQL_SCOPE_FOR);

    AstNode* iterator = _ast->createNodeIterator("x", 1, vertices);
    AstNode* xRef = _ast->createNodeReference("x");

    AstNode* expansion = _ast->createNodeExpansion(
        /*levels*/ 1, iterator, xRef,
        /*filter*/ nullptr, /*limit*/ nullptr, /*projection*/ nullptr);

    _ast->scopes()->endCurrent();

    AstNode* age = _ast->createNodeAttributeAccess(expansion, "age");
    AstNode* rhs = _ast->createNodeValueInt(value);
    AstNode* quantifier = _ast->createNodeQuantifier(Quantifier::Type::kAll);

    return _ast->createNodeBinaryArrayOperator(
        NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ, age, rhs, quantifier);
  }

  AstNode* cmpIntIn(Variable const* v, char const* attr,
                    std::vector<int64_t> values) {
    AstNode* access =
        _ast->createNodeAttributeAccess(_ast->createNodeReference(v), attr);
    AstNode* arr = _ast->createNodeArray(values.size());
    for (auto value : values) {
      arr->addMember(_ast->createNodeValueInt(value));
    }
    return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, access,
                                          arr);
  }

  AstNode* cmpIntNin(Variable const* v, char const* attr,
                     std::vector<int64_t> values) {
    AstNode* access =
        _ast->createNodeAttributeAccess(_ast->createNodeReference(v), attr);
    AstNode* arr = _ast->createNodeArray(values.size());
    for (auto value : values) {
      arr->addMember(_ast->createNodeValueInt(value));
    }
    return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, access,
                                          arr);
  }

  static std::string attrOf(AstNode const* cmp) {
    auto const* lhs = cmp->getMember(0);
    EXPECT_EQ(lhs->type, NODE_TYPE_ATTRIBUTE_ACCESS);
    return std::string(lhs->getStringView());
  }
};

// -----------------------------------------------------------------------------
// Pass 1: Fundamental behavior
// -----------------------------------------------------------------------------
TEST_F(ConditionRemoveForTraversalTest, ReturnsNullptrWhenRootIsNull) {
  // filter: nullptr
  // traversal: v.a == 1
  // => nullptr (early guard: root == nullptr)
  Condition filterCond(_ast);
  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  traversalCond.normalize();
  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);
  ASSERT_EQ(result, nullptr);
  ASSERT_EQ(filterCond.root(), nullptr);
}
TEST_F(ConditionRemoveForTraversalTest,
       ReturnsOriginalWhenTraversalConditionIsNull) {
  // filter: v.a == 1
  // traversal: nullptr
  // => unchanged (early guard: other == nullptr)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  filterCond.normalize();
  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);
  AstNode* result = removeTraversalCondition(filterCond, _plan, _v, nullptr,
                                             /*isPathCondition*/ false);
  ASSERT_EQ(result, before);
  ASSERT_EQ(filterCond.root(), before);
}
TEST_F(ConditionRemoveForTraversalTest, ReturnsOriginalForMultipleOrBranches) {
  // filter: v.a == 1 OR v.b == 2
  // traversal: v.a == 1 OR v.b == 2
  // => unchanged (removeTraversalCondition only handles one OR branch)
  AstNode* filterOr = _ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_OR,
      cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1),
      cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  Condition filterCond(_ast);
  filterCond.andCombine(filterOr);
  filterCond.normalize();
  AstNode* traversalOr = _ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_OR,
      cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1),
      cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  Condition traversalCond(_ast);
  traversalCond.andCombine(traversalOr);
  traversalCond.normalize();
  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);
  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);
  ASSERT_EQ(result, before);
}

// -----------------------------------------------------------------------------
// Pass 2: Vertex and edge conditions
// -----------------------------------------------------------------------------

TEST_F(ConditionRemoveForTraversalTest, RemovesIdenticalVertexCondition) {
  // filter: v.a == 1
  // traversal: v.a == 1
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest, RemovesCoveredMemberKeepsUncovered) {
  // filter: v.a == 1 AND v.b > 2
  // traversal: v.a == 1
  // => v.b > 2
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _v, "b", 2));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_GT);
  EXPECT_EQ(attrOf(result), "b");
}

TEST_F(ConditionRemoveForTraversalTest, RemovesAllCoveredMembers) {
  // filter: v.a == 1 AND v.b == 2
  // traversal: v.a == 1 AND v.b == 2
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest, KeepsDifferentAttribute) {
  // filter: v.a == 1
  // traversal: v.b == 1
  // => unchanged
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 1));
  traversalCond.normalize();

  AstNode* before = filterCond.root();
  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, before);
}

TEST_F(ConditionRemoveForTraversalTest, KeepsDifferentVariable) {
  // filter: e.a == 1
  // traversal: v.a == 1
  // => unchanged
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _e, "a", 1));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));
  traversalCond.normalize();

  AstNode* before = filterCond.root();
  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, before);
}

TEST_F(ConditionRemoveForTraversalTest, RemovesIdenticalEdgeCondition) {
  // filter: e.weight > 5
  // traversal: e.weight > 5
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _e, "weight", 5));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(
      cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _e, "weight", 5));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _e, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest, RemovesIdenticalInCondition) {
  // filter: v.a IN [1, 2]
  // traversal: v.a IN [1, 2]
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpIntIn(_v, "a", {1, 2}));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpIntIn(_v, "a", {1, 2}));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest,
       RemovesInConditionKeepsUncoveredMember) {
  // filter: v.a IN [1, 2] AND v.b == 3
  // traversal: v.a IN [1, 2]
  // => v.b == 3
  Condition filterCond(_ast);
  filterCond.andCombine(cmpIntIn(_v, "a", {1, 2}));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 3));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpIntIn(_v, "a", {1, 2}));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_EQ);
  EXPECT_EQ(attrOf(result), "b");
}

TEST_F(ConditionRemoveForTraversalTest, KeepsIdenticalNinCondition) {
  // filter: v.a NOT IN [1, 2]
  // traversal: v.a NOT IN [1, 2]
  // => unchanged (NIN falls into unsupported compare-table column)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpIntNin(_v, "a", {1, 2}));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpIntNin(_v, "a", {1, 2}));
  traversalCond.normalize();

  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, before);
}

// -----------------------------------------------------------------------------
// Pass 3: Implied coverage
// -----------------------------------------------------------------------------

TEST_F(ConditionRemoveForTraversalTest, EqualityImpliesRangeBounds) {
  // filter: v.a > 0 AND v.a < 10 AND v.a == 5
  // traversal: v.a == 5
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _v, "a", 0));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_LT, _v, "a", 10));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 5));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 5));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest, StrongerBoundImpliesWeakerBound) {
  // filter: v.a >= 5 AND v.a > 10
  // traversal: v.a > 10
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GE, _v, "a", 5));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _v, "a", 10));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _v, "a", 10));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest,
       NonConstantEqualityKeepsUncoveredMember) {
  // filter: v.a == e.x AND v.b > 3
  // traversal: v.a == e.x
  // => v.b > 3
  Condition filterCond(_ast);
  filterCond.andCombine(
      cmpAttr(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", _e, "x"));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _v, "b", 3));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(
      cmpAttr(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", _e, "x"));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_GT);
  EXPECT_EQ(attrOf(result), "b");
}

// -----------------------------------------------------------------------------
// Pass 4: Skipped operators
// -----------------------------------------------------------------------------

TEST_F(ConditionRemoveForTraversalTest, RemovesIdenticalNeCondition) {
  // filter: v.a != 1
  // traversal: v.a != 1
  // => nullptr (NE is allowed for traversal)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_NE, _v, "a", 1));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_NE, _v, "a", 1));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest, KeepsNonComparisonOp) {
  AstNode* notNode = _ast->createNodeUnaryOperator(
      NODE_TYPE_OPERATOR_UNARY_NOT,
      cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "a", 1));

  // filter: NOT (v.a == 1) AND v.b == 2
  // traversal: v.b == 2
  // => NOT (v.a == 1)
  Condition filterCond(_ast);
  filterCond.andCombine(notNode);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _v, "b", 2));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _v, traversalCond.root(),
                               /*isPathCondition*/ false);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_UNARY_NOT);
}

// -----------------------------------------------------------------------------
// Pass 5: Path conditions
// -----------------------------------------------------------------------------

TEST_F(ConditionRemoveForTraversalTest,
       RemovesPathArrayComparisonWhenPathCondition) {
  // filter: p.vertices[*].age ALL == 5
  // traversal: p.vertices[*].age ALL == 5
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(pathArrayEq(_p, 5));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(pathArrayEq(_p, 5));
  traversalCond.normalize();

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _p, traversalCond.root(),
                               /*isPathCondition*/ true);

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForTraversalTest,
       KeepsPathArrayComparisonWhenNotPathCondition) {
  // filter: p.vertices[*].age ALL == 5
  // traversal: p.vertices[*].age ALL == 5
  // => unchanged (array comparisons are only allowed for path conditions)
  Condition filterCond(_ast);
  filterCond.andCombine(pathArrayEq(_p, 5));
  filterCond.normalize();

  Condition traversalCond(_ast);
  traversalCond.andCombine(pathArrayEq(_p, 5));
  traversalCond.normalize();

  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);

  AstNode* result =
      removeTraversalCondition(filterCond, _plan, _p, traversalCond.root(),
                               /*isPathCondition*/ false);

  EXPECT_EQ(result, before);
}
}  // namespace
}  // namespace arangodb::aql