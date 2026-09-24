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

#pragma once

#include "gtest/gtest.h"
#include "Mocks/Servers.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/BindParameters.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Match/PatternNormalizer.h"
#include "Aql/Parser/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Aql/TypedAstNodes.h"
#include "Containers/SmallVector.h"
#include "Transaction/OperationOrigin.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>

namespace arangodb::tests::aql::match {

/// @brief Shared MATCH parse / normalize / plan fixture (COR-893).
class MatchTestFixture : public ::testing::Test {
 protected:
  static void createDocumentCollection(std::string_view name) {
    auto& vocbase = server->getSystemDatabase();
    velocypack::Builder builder;
    builder.openObject();
    builder.add("name", velocypack::Value(name));
    builder.close();
    vocbase.createCollection(builder.slice());
  }

  static void createEdgeCollection(std::string_view name) {
    auto& vocbase = server->getSystemDatabase();
    velocypack::Builder builder;
    builder.openObject();
    builder.add("name", velocypack::Value(name));
    builder.add("type", velocypack::Value(static_cast<int>(TRI_COL_TYPE_EDGE)));
    builder.close();
    vocbase.createCollection(builder.slice());
  }

  static void SetUpTestCase() {
    server = std::make_unique<mocks::MockRestAqlServer>();
    createDocumentCollection("vc");
    createEdgeCollection("ec");
    createEdgeCollection("ec2");
    createDocumentCollection("resolved_vc");
    createDocumentCollection("resolved_ec");
    createDocumentCollection("mvc");
    createDocumentCollection("mec1");
    createDocumentCollection("mec2");
  }

  static void TearDownTestCase() { server.reset(); }

  struct ParsedMatch {
    std::unique_ptr<arangodb::aql::QueryContext> queryContext;
    std::unique_ptr<arangodb::aql::Ast> ast;
    /// @brief must outlive Ast collection nodes created from @@ bind params.
    std::unique_ptr<arangodb::aql::BindParameters> bindParameters;
    arangodb::aql::AstNode const* matchNode;
  };

  static std::unique_ptr<arangodb::aql::BindParameters> makeBindParameters(
      arangodb::ResourceMonitor& resourceMonitor,
      std::initializer_list<std::pair<char const*, char const*>> params) {
    auto builder = std::make_shared<velocypack::Builder>();
    builder->openObject();
    for (auto const& [key, value] : params) {
      builder->add(key, velocypack::Value(value));
    }
    builder->close();
    return std::make_unique<arangodb::aql::BindParameters>(resourceMonitor,
                                                           std::move(builder));
  }

  ParsedMatch parseMatch(
      std::string_view query, bool injectBindParameters = false,
      std::initializer_list<std::pair<char const*, char const*>> bindParams =
          {}) {
    using namespace arangodb::aql;
    auto& vocbase = server->getSystemDatabase();
    auto queryContext = StandaloneCalculation::buildQueryContext(
        vocbase, transaction::OperationOriginTestCase{});
    queryContext->queryOptions().enableMatchStatement = "experimental";

    auto ast = std::make_unique<Ast>(*queryContext);
    auto queryString = QueryString(query);
    Parser parser(*queryContext, &queryContext->warnings(), *ast, queryString);
    parser.parse();

    std::unique_ptr<BindParameters> parameters;
    if (injectBindParameters) {
      parameters =
          makeBindParameters(queryContext->resourceMonitor(), bindParams);
      ast->injectBindParametersFirstStage(*parameters,
                                          queryContext->resolver());
      ast->injectBindParametersSecondStage(*parameters);
    }

    AstNode const* matchNode = nullptr;
    for (size_t i = 0; i < ast->root()->numMembers(); ++i) {
      AstNode const* op = ast->root()->getMember(i);
      if (op->type == NODE_TYPE_MATCH) {
        matchNode = op;
        break;
      }
    }
    EXPECT_NE(nullptr, matchNode);

    return ParsedMatch{std::move(queryContext), std::move(ast),
                       std::move(parameters), matchNode};
  }

  static arangodb::aql::match::NormalizedStatement normalize(
      ParsedMatch const& parsed) {
    arangodb::aql::match::PatternNormalizer normalizer(*parsed.ast);
    return normalizer.normalize(
        arangodb::aql::ast::MatchNode(parsed.matchNode));
  }

  static std::unique_ptr<arangodb::aql::ExecutionPlan> instantiatePlan(
      ParsedMatch const& parsed) {
    return arangodb::aql::ExecutionPlan::instantiateFromAst(parsed.ast.get(),
                                                            false);
  }

  static void optimizeAst(ParsedMatch& parsed) {
    parsed.ast->validateAndOptimize(
        parsed.queryContext->trxForOptimization(),
        arangodb::aql::Ast::ValidateAndOptimizeOptions{});
  }

  static size_t countNodesOfType(arangodb::aql::ExecutionPlan const& plan,
                                 arangodb::aql::ExecutionNode::NodeType type) {
    arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*, 8> nodes;
    plan.findNodesOfType(nodes, type, true);
    return nodes.size();
  }

  static inline std::unique_ptr<mocks::MockRestAqlServer> server;
};

}  // namespace arangodb::tests::aql::match
