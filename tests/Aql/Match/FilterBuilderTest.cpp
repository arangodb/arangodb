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
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Match/FilterBuilder.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Mocks/Servers.h"

#include <unordered_map>

using namespace arangodb::aql;
using namespace arangodb::aql::match;

namespace {

class FilterBuilderTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  std::shared_ptr<Query> query{server.createFakeQuery()};
  Ast ast{*query};
  ExecutionPlan plan{&ast, false};
  FilterBuilder filters{plan, &ast};
};

}  // namespace

TEST_F(FilterBuilderTest, createPropertyAccessBuildsAttributeAccess) {
  Variable const* v = ast.variables()->createTemporaryVariable();
  AstNode* access = filters.createPropertyAccess(v, "_id");
  ASSERT_NE(nullptr, access);
  EXPECT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, access->type);
  EXPECT_EQ("_id", access->getStringView());
}

TEST_F(FilterBuilderTest, createPropertiesFilterEmptyIsTrue) {
  Variable const* v = ast.variables()->createTemporaryVariable();
  std::unordered_map<VariableId, Variable const*> subst;
  auto [calc, filter] =
      filters.createPropertiesFilter(v, {}, std::nullopt, subst);
  ASSERT_NE(nullptr, calc);
  ASSERT_NE(nullptr, filter);
  ASSERT_NE(nullptr, calc->expression());
  ASSERT_NE(nullptr, calc->expression()->node());
  EXPECT_EQ(NODE_TYPE_VALUE, calc->expression()->node()->type);
  EXPECT_TRUE(calc->expression()->node()->isTrue());
}

TEST_F(FilterBuilderTest, createPropertiesFilterEqualsProperty) {
  Variable const* v = ast.variables()->createTemporaryVariable();
  auto* value = ast.createNodeValueString("alice", 5);
  std::vector<PropertyConstraint> properties{{"name", ExpressionRef{value}}};
  std::unordered_map<VariableId, Variable const*> subst;
  auto [calc, filter] =
      filters.createPropertiesFilter(v, properties, std::nullopt, subst);
  ASSERT_NE(nullptr, calc->expression()->node());
  EXPECT_EQ(NODE_TYPE_OPERATOR_BINARY_EQ, calc->expression()->node()->type);
}

TEST_F(FilterBuilderTest, createVertexEdgeFilterOutboundIsAnd) {
  Variable const* left = ast.variables()->createTemporaryVariable();
  Variable const* edge = ast.variables()->createTemporaryVariable();
  Variable const* right = ast.variables()->createTemporaryVariable();
  auto [calc, filter] = filters.createVertexEdgeFilter(
      left, edge, right, EdgeDirection::kOutbound);
  ASSERT_NE(nullptr, calc->expression()->node());
  EXPECT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, calc->expression()->node()->type);
}

TEST_F(FilterBuilderTest, createVertexEdgeFilterInboundIsAnd) {
  Variable const* left = ast.variables()->createTemporaryVariable();
  Variable const* edge = ast.variables()->createTemporaryVariable();
  Variable const* right = ast.variables()->createTemporaryVariable();
  auto [calc, filter] = filters.createVertexEdgeFilter(left, edge, right,
                                                       EdgeDirection::kInbound);
  EXPECT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, calc->expression()->node()->type);
}

TEST_F(FilterBuilderTest, createVertexEdgeFilterAnyIsOr) {
  Variable const* left = ast.variables()->createTemporaryVariable();
  Variable const* edge = ast.variables()->createTemporaryVariable();
  Variable const* right = ast.variables()->createTemporaryVariable();
  auto [calc, filter] =
      filters.createVertexEdgeFilter(left, edge, right, EdgeDirection::kAny);
  EXPECT_EQ(NODE_TYPE_OPERATOR_BINARY_OR, calc->expression()->node()->type);
}
