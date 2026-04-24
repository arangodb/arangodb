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
};

// -----------------------------------------------------------------------------
// Pass 1: Fundamental guards
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

}  // namespace
}  // namespace arangodb::aql