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
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/ErrorCode.h"
#include "Mocks/Servers.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace arangodb::aql;
using namespace arangodb::aql::match;

namespace {

class ProjectionBuilderTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  std::shared_ptr<Query> query{server.createFakeQuery()};
  Ast ast{*query};
  ExecutionPlan plan{&ast, false};
  ProjectionBuilder projections{plan, &ast};

  static std::unordered_set<std::string> objectKeys(AstNode const* node) {
    std::unordered_set<std::string> keys;
    for (size_t i = 0; i < node->numMembers(); ++i) {
      AstNode const* member = node->getMemberUnchecked(i);
      if (member->type == NODE_TYPE_OBJECT_ELEMENT) {
        keys.emplace(member->getStringView());
      }
    }
    return keys;
  }
};

}  // namespace

TEST_F(ProjectionBuilderTest, identityProjectionReferencesFullDocument) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  std::unordered_map<VariableId, Variable const*> subst;
  auto* node = projections.createPatternProjection(
      dest, full, nullptr, kMandatoryDocumentProjectionAttributes, subst);
  ASSERT_NE(nullptr, node);
  EXPECT_EQ(ExecutionNode::CALCULATION, node->getType());
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  ASSERT_EQ(NODE_TYPE_REFERENCE, calc->expression()->node()->type);
}

TEST_F(ProjectionBuilderTest, documentKeepInjectsMandatoryId) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"name"}));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createDocumentPatternProjection(dest, full,
                                                           projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  AstNode const* root = calc->expression()->node();
  ASSERT_EQ(NODE_TYPE_OBJECT, root->type);
  auto keys = objectKeys(root);
  EXPECT_TRUE(keys.contains("_id"));
  EXPECT_TRUE(keys.contains("name"));
}

TEST_F(ProjectionBuilderTest, documentKeepSkipsReservedId) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"_id"}));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createDocumentPatternProjection(dest, full,
                                                           projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  AstNode const* root = calc->expression()->node();
  EXPECT_EQ(1U, root->numMembers());
  EXPECT_EQ("_id", root->getMember(0)->getStringView());
}

TEST_F(ProjectionBuilderTest, edgeProjectionInjectsIdFromTo) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"weight"}));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createEdgeDocumentPatternProjection(
      dest, full, projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  auto keys = objectKeys(calc->expression()->node());
  EXPECT_TRUE(keys.contains("_id"));
  EXPECT_TRUE(keys.contains("_from"));
  EXPECT_TRUE(keys.contains("_to"));
  EXPECT_TRUE(keys.contains("weight"));
}

namespace {

AstNode const* objectElementValue(AstNode const* object, std::string_view key) {
  for (size_t i = 0; i < object->numMembers(); ++i) {
    AstNode const* member = object->getMemberUnchecked(i);
    if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
        member->getStringView() == key) {
      return member->getMember(0);
    }
  }
  return nullptr;
}

}  // namespace

TEST_F(ProjectionBuilderTest, parentKeepSubsumesChildPath) {
  // KEEP profile, profile.name → whole profile object, not a nested name.
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"profile", "name"}));
  projection.items.push_back(ProjectionItem::keepPath({"profile"}));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createDocumentPatternProjection(dest, full,
                                                           projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  AstNode const* root = calc->expression()->node();
  auto keys = objectKeys(root);
  EXPECT_TRUE(keys.contains("_id"));
  EXPECT_TRUE(keys.contains("profile"));
  EXPECT_EQ(2U, keys.size());

  AstNode const* profile = objectElementValue(root, "profile");
  ASSERT_NE(nullptr, profile);
  EXPECT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, profile->type);
}

TEST_F(ProjectionBuilderTest, deeperParentKeepSubsumesChildPath) {
  // KEEP a.b, a.b.c.d → keep a.b only.
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"a", "b", "c", "d"}));
  projection.items.push_back(ProjectionItem::keepPath({"a", "b"}));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createDocumentPatternProjection(dest, full,
                                                           projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  AstNode const* root = calc->expression()->node();
  AstNode const* a = objectElementValue(root, "a");
  ASSERT_NE(nullptr, a);
  EXPECT_EQ(NODE_TYPE_OBJECT, a->type);
  AstNode const* b = objectElementValue(a, "b");
  ASSERT_NE(nullptr, b);
  EXPECT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, b->type);
  EXPECT_EQ(nullptr, objectElementValue(a, "c"));
}

TEST_F(ProjectionBuilderTest, quotedLiteralKeepIsNotSubsumedByParentPath) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"profile"}));
  projection.items.push_back(ProjectionItem::keepLiteral("profile.name"));
  std::unordered_map<VariableId, Variable const*> subst;

  auto* node = projections.createDocumentPatternProjection(dest, full,
                                                           projection, subst);
  auto* calc = ExecutionNode::castTo<CalculationNode*>(node);
  auto keys = objectKeys(calc->expression()->node());
  EXPECT_TRUE(keys.contains("profile"));
  EXPECT_TRUE(keys.contains("profile.name"));
}

TEST_F(ProjectionBuilderTest, aliasOverwritingSystemAttributeThrows) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  std::unordered_map<VariableId, Variable const*> subst;

  auto expectThrows = [&](Projection const& projection, bool edge) {
    try {
      if (edge) {
        (void)projections.createEdgeDocumentPatternProjection(
            dest, full, projection, subst);
      } else {
        (void)projections.createDocumentPatternProjection(dest, full,
                                                          projection, subst);
      }
      FAIL() << "expected system attribute alias to throw";
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_QUERY_PARSE, ex.code());
    }
  };

  for (std::string_view name : {"_id", "_key", "_rev", "_from", "_to"}) {
    Projection projection;
    projection.items.push_back(ProjectionItem::alias(
        std::string(name), ExpressionRef{ast.createNodeValueString("foo", 3)}));
    expectThrows(projection, false);
    expectThrows(projection, true);
  }
}

TEST_F(ProjectionBuilderTest, aliasCollisionWithKeepThrows) {
  Variable const* dest = ast.variables()->createTemporaryVariable();
  Variable const* full = ast.variables()->createTemporaryVariable();
  Projection projection;
  projection.items.push_back(ProjectionItem::keepPath({"name"}));
  projection.items.push_back(
      ProjectionItem::alias("name", ExpressionRef{ast.createNodeValueInt(1)}));
  std::unordered_map<VariableId, Variable const*> subst;

  try {
    (void)projections.createDocumentPatternProjection(dest, full, projection,
                                                      subst);
    FAIL() << "expected duplicate projection attribute to throw";
  } catch (arangodb::basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_QUERY_PARSE, ex.code());
  }
}
