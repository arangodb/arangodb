////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/Parser.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Aql/Optimizer/Rules/ExpressionMatcher.h"
#include "Aql/QueryContext.h"
#include "Logger/LogMacros.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include "Mocks/Servers.h"

using namespace arangodb::aql;
using namespace arangodb::velocypack;
using namespace arangodb::tests;

namespace {
auto json(AstNode* node) -> std::string {
  VPackBuilder b;
  node->toVelocyPack(b, true);
  return b.toString();
}

}  // namespace

struct TestContext {
  explicit TestContext(std::string to_parse)
      : server{},
        query{server.createFakeQuery()},
        trx{server.createFakeTransaction()} {
    // TODO: a lot of magic is happening here that is strictly speaking
    // entirely unnecessary.
    auto qs = QueryString(to_parse);
    auto parser = arangodb::aql::Parser(*query, *query->ast(), qs);
    parser.parse();

    // TODO: find out whether this validateAndOptimize step is necessary for
    // these tests
    query->ast()->validateAndOptimize(*trx, {.optimizeNonCacheable = true});
  }

  auto getTopNode() -> AstNode* {
    return query->ast()->root()->getMemberUnchecked(1)->getMemberUnchecked(0);
  }

  mocks::MockAqlServer server;
  std::shared_ptr<Query> query;
  std::shared_ptr<arangodb::transaction::Methods> trx;
};

TEST(ExpressionMatcherTest, matches_value_node) {
  auto expression = TestContext(R"=( RETURN 1 )=");

  auto* node = expression.getTopNode();

  {
    auto match = expression_matcher::matchValueNode(node);
    ASSERT_TRUE(match.has_value());
  }

  {
    auto match = expression_matcher::matchValueNode(
        node, AstNodeValueType::VALUE_TYPE_INT);
    ASSERT_TRUE(match.has_value());
  }

  {
    auto match = expression_matcher::matchValueNode(
        node, AstNodeValueType::VALUE_TYPE_INT, 1);
    ASSERT_TRUE(match.has_value());
  }

  {
    auto match = expression_matcher::matchValueNode(
        node, AstNodeValueType::VALUE_TYPE_INT, 2);
    ASSERT_FALSE(match.has_value());
  }
}

TEST(ExpressionMatcherTest, matches_filter_expression) {
  auto expression = TestContext(
      R"=(RETURN path.vertices[* RETURN CURRENT.colour == "green"] ALL == true)=");

  auto* node = expression.getTopNode();

  {
    auto match = expression_matcher::matchNodeType(
        node, AstNodeType::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ);
    ASSERT_TRUE(match.has_value()) << json(node);
  }
}
