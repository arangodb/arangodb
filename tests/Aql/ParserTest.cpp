////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Mocks/Servers.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Transaction/OperationOrigin.h"
#include "Logger/LogMacros.h"

#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

class ParserTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    server = std::make_unique<mocks::MockRestAqlServer>();
  }
  static void TearDownTestCase() { server.reset(); }

 protected:
  static inline std::unique_ptr<mocks::MockRestAqlServer> server;
};

TEST_F(ParserTest, parseSimpleTernaryCondition) {
  auto& vocbase = server->getSystemDatabase();

  auto queryContext = aql::StandaloneCalculation::buildQueryContext(
      vocbase, transaction::OperationOriginTestCase{});
  auto ast = aql::Ast(*queryContext);
  auto queryString =
      aql::QueryString(std::string_view("RETURN true ? 'true' : 'false'"));

  aql::Parser parser(*queryContext, ast, queryString);
  ASSERT_FALSE(parser.forceInlineTernary());
  parser.parse();

  aql::AstNode const* rootNode = ast.root();
  ASSERT_EQ(aql::NODE_TYPE_ROOT, rootNode->type);
  ASSERT_EQ(2, rootNode->numMembers());

  aql::AstNode const* letNode = rootNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_LET, letNode->type);
  ASSERT_EQ(2, letNode->numMembers());
  aql::AstNode const* variableNode = letNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
  aql::AstNode const* valueNode = letNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, valueNode->type);
  ASSERT_TRUE(valueNode->isTrue());

  aql::AstNode const* returnNode = rootNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
  ASSERT_EQ(1, returnNode->numMembers());

  aql::AstNode const* ternaryNode = returnNode->getMember(0);
  ASSERT_EQ(3, ternaryNode->numMembers());

  aql::AstNode const* conditionNode = ternaryNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, conditionNode->type);
  ASSERT_TRUE(conditionNode->isTrue());

  aql::AstNode const* truePart = ternaryNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, truePart->type);
  ASSERT_TRUE(truePart->isStringValue());
  ASSERT_EQ("true", truePart->getStringView());

  aql::AstNode const* falsePart = ternaryNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, falsePart->type);
  ASSERT_TRUE(falsePart->isStringValue());
  ASSERT_EQ("false", falsePart->getStringView());
}

TEST_F(ParserTest, parseSimpleTernaryConditionForceInline) {
  auto& vocbase = server->getSystemDatabase();

  auto queryContext = aql::StandaloneCalculation::buildQueryContext(
      vocbase, transaction::OperationOriginTestCase{});
  auto ast = aql::Ast(*queryContext);
  auto queryString =
      aql::QueryString(std::string_view("RETURN true ? 'true' : 'false'"));

  aql::Parser parser(*queryContext, ast, queryString);
  parser.setForceInlineTernary();
  ASSERT_TRUE(parser.forceInlineTernary());
  parser.parse();

  aql::AstNode const* rootNode = ast.root();
  ASSERT_EQ(aql::NODE_TYPE_ROOT, rootNode->type);
  ASSERT_EQ(1, rootNode->numMembers());

  aql::AstNode const* returnNode = rootNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
  ASSERT_EQ(1, returnNode->numMembers());

  aql::AstNode const* ternaryNode = returnNode->getMember(0);
  ASSERT_EQ(3, ternaryNode->numMembers());

  aql::AstNode const* conditionNode = ternaryNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, conditionNode->type);
  ASSERT_TRUE(conditionNode->isTrue());

  aql::AstNode const* truePart = ternaryNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, truePart->type);
  ASSERT_TRUE(truePart->isStringValue());
  ASSERT_EQ("true", truePart->getStringView());

  aql::AstNode const* falsePart = ternaryNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, falsePart->type);
  ASSERT_TRUE(falsePart->isStringValue());
  ASSERT_EQ("false", falsePart->getStringView());
}

TEST_F(ParserTest, parseTernaryWithSubquery) {
  auto& vocbase = server->getSystemDatabase();

  auto queryContext = aql::StandaloneCalculation::buildQueryContext(
      vocbase, transaction::OperationOriginTestCase{});
  auto ast = aql::Ast(*queryContext);
  auto queryString = aql::QueryString(std::string_view(
      "RETURN true ? (FOR i IN 1..10 RETURN i) : (FOR j IN 1..2 RETURN j)"));

  aql::Parser parser(*queryContext, ast, queryString);
  ASSERT_FALSE(parser.forceInlineTernary());
  parser.parse();

  aql::AstNode const* rootNode = ast.root();
  ASSERT_EQ(aql::NODE_TYPE_ROOT, rootNode->type);
  ASSERT_EQ(4, rootNode->numMembers());

  aql::AstNode const* letNode = rootNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_LET, letNode->type);
  ASSERT_EQ(2, letNode->numMembers());
  aql::AstNode const* variableNode = letNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
  aql::Variable const* condVar =
      static_cast<Variable const*>(variableNode->getData());

  aql::AstNode const* valueNode = letNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, valueNode->type);
  ASSERT_TRUE(valueNode->isTrue());

  // subquery 1
  aql::Variable const* trueVar = nullptr;
  aql::AstNode const* sq1Node = rootNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_LET, sq1Node->type);
  ASSERT_EQ(2, sq1Node->numMembers());
  {
    aql::AstNode const* variableNode = sq1Node->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
    trueVar = static_cast<Variable const*>(variableNode->getData());
    aql::AstNode const* sqNode = sq1Node->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_SUBQUERY, sqNode->type);
    ASSERT_EQ(3, sqNode->numMembers());

    aql::AstNode const* filterNode = sqNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_FILTER, filterNode->type);
    ASSERT_EQ(1, filterNode->numMembers());
    aql::AstNode const* filterExprNode = filterNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, filterExprNode->type);
    ASSERT_EQ(condVar, static_cast<Variable const*>(filterExprNode->getData()));

    aql::AstNode const* forNode = sqNode->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_FOR, forNode->type);
    aql::AstNode const* forVarNode = forNode->getMember(0);
    ASSERT_EQ("i", static_cast<Variable const*>(forVarNode->getData())->name);

    aql::AstNode const* returnNode = sqNode->getMember(2);
    ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
    ASSERT_EQ(1, returnNode->numMembers());
    aql::AstNode const* returnExprNode = returnNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, returnExprNode->type);
    ASSERT_EQ("i",
              static_cast<Variable const*>(returnExprNode->getData())->name);
  }

  // subquery 2
  aql::Variable const* falseVar = nullptr;
  aql::AstNode const* sq2Node = rootNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_LET, sq2Node->type);
  ASSERT_EQ(2, sq2Node->numMembers());
  {
    aql::AstNode const* variableNode = sq2Node->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
    falseVar = static_cast<Variable const*>(variableNode->getData());
    aql::AstNode const* sqNode = sq2Node->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_SUBQUERY, sqNode->type);
    ASSERT_EQ(3, sqNode->numMembers());

    aql::AstNode const* filterNode = sqNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_FILTER, filterNode->type);
    ASSERT_EQ(1, filterNode->numMembers());
    aql::AstNode const* notNode = filterNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_OPERATOR_UNARY_NOT, notNode->type);
    ASSERT_EQ(1, notNode->numMembers());
    aql::AstNode const* filterExprNode = notNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, filterExprNode->type);
    ASSERT_EQ(condVar, static_cast<Variable const*>(filterExprNode->getData()));

    aql::AstNode const* forNode = sqNode->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_FOR, forNode->type);
    aql::AstNode const* forVarNode = forNode->getMember(0);
    ASSERT_EQ("j", static_cast<Variable const*>(forVarNode->getData())->name);

    aql::AstNode const* returnNode = sqNode->getMember(2);
    ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
    ASSERT_EQ(1, returnNode->numMembers());
    aql::AstNode const* returnExprNode = returnNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, returnExprNode->type);
    ASSERT_EQ("j",
              static_cast<Variable const*>(returnExprNode->getData())->name);
  }

  // RETURN trueVar ? truePart : falsePart
  aql::AstNode const* returnNode = rootNode->getMember(3);
  ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
  ASSERT_EQ(1, returnNode->numMembers());

  aql::AstNode const* ternaryNode = returnNode->getMember(0);
  ASSERT_EQ(3, ternaryNode->numMembers());

  aql::AstNode const* conditionNode = ternaryNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, conditionNode->type);
  ASSERT_TRUE(conditionNode->isTrue());

  aql::AstNode const* truePart = ternaryNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_REFERENCE, truePart->type);
  ASSERT_EQ(trueVar, static_cast<Variable const*>(truePart->getData()));

  aql::AstNode const* falsePart = ternaryNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_REFERENCE, falsePart->type);
  ASSERT_EQ(falseVar, static_cast<Variable const*>(falsePart->getData()));
}

TEST_F(ParserTest, parseTernaryWithSubqueryForceInline) {
  auto& vocbase = server->getSystemDatabase();

  auto queryContext = aql::StandaloneCalculation::buildQueryContext(
      vocbase, transaction::OperationOriginTestCase{});
  auto ast = aql::Ast(*queryContext);
  auto queryString = aql::QueryString(std::string_view(
      "RETURN true ? (FOR i IN 1..10 RETURN i) : (FOR j IN 1..2 RETURN j)"));

  aql::Parser parser(*queryContext, ast, queryString);
  parser.setForceInlineTernary();
  ASSERT_TRUE(parser.forceInlineTernary());
  parser.parse();

  aql::AstNode const* rootNode = ast.root();
  ASSERT_EQ(aql::NODE_TYPE_ROOT, rootNode->type);
  ASSERT_EQ(3, rootNode->numMembers());

  // subquery 1
  aql::Variable const* trueVar = nullptr;
  aql::AstNode const* sq1Node = rootNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_LET, sq1Node->type);
  ASSERT_EQ(2, sq1Node->numMembers());
  {
    aql::AstNode const* variableNode = sq1Node->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
    trueVar = static_cast<Variable const*>(variableNode->getData());
    aql::AstNode const* sqNode = sq1Node->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_SUBQUERY, sqNode->type);
    ASSERT_EQ(2, sqNode->numMembers());

    aql::AstNode const* forNode = sqNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_FOR, forNode->type);
    aql::AstNode const* forVarNode = forNode->getMember(0);
    ASSERT_EQ("i", static_cast<Variable const*>(forVarNode->getData())->name);

    aql::AstNode const* returnNode = sqNode->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
    ASSERT_EQ(1, returnNode->numMembers());
    aql::AstNode const* returnExprNode = returnNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, returnExprNode->type);
    ASSERT_EQ("i",
              static_cast<Variable const*>(returnExprNode->getData())->name);
  }

  // subquery 2
  aql::Variable const* falseVar = nullptr;
  aql::AstNode const* sq2Node = rootNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_LET, sq2Node->type);
  ASSERT_EQ(2, sq2Node->numMembers());
  {
    aql::AstNode const* variableNode = sq2Node->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_VARIABLE, variableNode->type);
    falseVar = static_cast<Variable const*>(variableNode->getData());
    aql::AstNode const* sqNode = sq2Node->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_SUBQUERY, sqNode->type);
    ASSERT_EQ(2, sqNode->numMembers());

    aql::AstNode const* forNode = sqNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_FOR, forNode->type);
    aql::AstNode const* forVarNode = forNode->getMember(0);
    ASSERT_EQ("j", static_cast<Variable const*>(forVarNode->getData())->name);

    aql::AstNode const* returnNode = sqNode->getMember(1);
    ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
    ASSERT_EQ(1, returnNode->numMembers());
    aql::AstNode const* returnExprNode = returnNode->getMember(0);
    ASSERT_EQ(aql::NODE_TYPE_REFERENCE, returnExprNode->type);
    ASSERT_EQ("j",
              static_cast<Variable const*>(returnExprNode->getData())->name);
  }

  // RETURN trueVar ? truePart : falsePart
  aql::AstNode const* returnNode = rootNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_RETURN, returnNode->type);
  ASSERT_EQ(1, returnNode->numMembers());

  aql::AstNode const* ternaryNode = returnNode->getMember(0);
  ASSERT_EQ(3, ternaryNode->numMembers());

  aql::AstNode const* conditionNode = ternaryNode->getMember(0);
  ASSERT_EQ(aql::NODE_TYPE_VALUE, conditionNode->type);
  ASSERT_TRUE(conditionNode->isTrue());

  aql::AstNode const* truePart = ternaryNode->getMember(1);
  ASSERT_EQ(aql::NODE_TYPE_REFERENCE, truePart->type);
  ASSERT_EQ(trueVar, static_cast<Variable const*>(truePart->getData()));

  aql::AstNode const* falsePart = ternaryNode->getMember(2);
  ASSERT_EQ(aql::NODE_TYPE_REFERENCE, falsePart->type);
  ASSERT_EQ(falseVar, static_cast<Variable const*>(falsePart->getData()));
}
