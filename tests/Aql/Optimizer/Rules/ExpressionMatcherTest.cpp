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

#include <vector>
#include <ranges>

using namespace arangodb::aql;
using namespace arangodb::velocypack;
using namespace arangodb::tests;

using namespace arangodb::aql::expression_matcher;

namespace {
auto json(AstNode const* node) -> std::string {
  VPackBuilder b;
  node->toVelocyPack(b, true);
  return b.toString();
}

auto ast_printer(AstNode const* node, size_t indent) -> std::string;

auto node_printer(AstNode const* node, size_t indent) -> std::string {
  auto pad = std::string(indent, ' ');
  auto result = std::string{};
  switch (node->type) {
    case NODE_TYPE_EXPANSION: {
      result += "\n";
      result += pad + fmt::format(
                          "iterator: {}\n",
                          ast_printer(node->getMemberUnchecked(0), indent + 2));
      result += pad + fmt::format(
                          "variable: {}\n",
                          ast_printer(node->getMemberUnchecked(1), indent + 2));
      result += pad + fmt::format(
                          "filter: {}\n",
                          ast_printer(node->getMemberUnchecked(2), indent + 2));
      result += pad + fmt::format(
                          "limit: {}\n",
                          ast_printer(node->getMemberUnchecked(3), indent + 2));
      result += pad + fmt::format(
                          "map: {}",
                          ast_printer(node->getMemberUnchecked(4), indent + 2));
    }
    default: {
      if (auto members = node->numMembers(); members > 0) {
        for (size_t i = 0; i < members; ++i) {
          result += "\n";
          result += pad + ast_printer(node->getMemberUnchecked(i), indent + 2);
        }
      }
    }
  }

  return result;
}

auto ast_printer(AstNode const* node, size_t indent) -> std::string {
  return fmt::format("(|{}| ({}){})", node->getTypeString(), node->type,
                     node_printer(node, indent));
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

    // TODO: find out whether this validateAndOptimize step is necessary (or
    // counterproductive) for these tests
    // query->ast()->validateAndOptimize(*trx, {.optimizeNonCacheable = true});
  }

  auto getTopNode() -> AstNode* {
    return query->ast()->root()->getMemberUnchecked(2)->getMemberUnchecked(0);
  }

  mocks::MockAqlServer server;
  std::shared_ptr<Query> query;
  std::shared_ptr<arangodb::transaction::Methods> trx;
};

TEST(ExpressionMatcherTest, matches_type) {
  auto node = AstNode(NODE_TYPE_NOP);

  {
    // Has to succeed
    auto result = MatchNodeType(NODE_TYPE_NOP).apply(&node);
    ASSERT_TRUE(result.success());
  }

  {
    // Has to fail
    auto result = MatchNodeType(NODE_TYPE_EXPANSION).apply(&node);
    ASSERT_TRUE(result.isError());
  }
}

TEST(ExpressionMatcherTest, matches_filter_expression) {
  auto expression = TestContext(
      R"=(LET path = [] RETURN path.vertices[* RETURN CURRENT.f == "green"] ALL == true)=");

  auto* node = expression.getTopNode();
  {
    auto matcher = arrayEq(  //
        expansion(           //
            iterator(
                AnyVariable{},                                            //
                attributeAccess(Reference{.name = "path"}, "vertices")),  //
            Reference{.name = "3_"},                                      //
            NoOp{},                                                       //
            NoOp{},                                                       //
            matchWithName("map", Any{})),                                 //
        AnyValue{},                                                       //
        expression_matcher::Quantifier{
            .which = ::arangodb::aql::Quantifier::Type::kAll});

    auto result = matcher.apply(node);
    ASSERT_TRUE(result.success());

    auto mapNode = result.matches().at("map");
    std::cerr << json(mapNode) << std::endl;
  }
}
