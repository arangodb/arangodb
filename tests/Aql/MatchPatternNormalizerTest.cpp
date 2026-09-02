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
#include "Mocks/Servers.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/BindParameters.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/MatchPatternNormalizer.h"
#include "Aql/Parser/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Transaction/OperationOrigin.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

namespace {

class MatchPatternNormalizerTest : public ::testing::Test {
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
    std::unique_ptr<QueryContext> queryContext;
    std::unique_ptr<Ast> ast;
    /// @brief must outlive Ast collection nodes created from @@ bind params.
    /// Ast::injectBindParametersFirstStage stores collection-name string_views
    /// that point into BindParameters' VPack Builder (same ownership model as
    /// Query::_bindParameters in production). Destroying bind parameters
    /// before reading those AST strings is a use-after-free.
    std::unique_ptr<BindParameters> bindParameters;
    AstNode const* matchNode;
  };

  static std::unique_ptr<BindParameters> makeBindParameters(
      ResourceMonitor& resourceMonitor,
      std::initializer_list<std::pair<char const*, char const*>> params) {
    auto builder = std::make_shared<velocypack::Builder>();
    builder->openObject();
    for (auto const& [key, value] : params) {
      builder->add(key, velocypack::Value(value));
    }
    builder->close();
    return std::make_unique<BindParameters>(resourceMonitor,
                                            std::move(builder));
  }

  ParsedMatch parseMatch(
      std::string_view query, bool injectBindParameters = false,
      std::initializer_list<std::pair<char const*, char const*>> bindParams =
          {}) {
    auto& vocbase = server->getSystemDatabase();
    auto queryContext = StandaloneCalculation::buildQueryContext(
        vocbase, transaction::OperationOriginTestCase{});
    queryContext->queryOptions().enableMatchStatement = "experimental";

    auto ast = std::make_unique<Ast>(*queryContext);
    auto queryString = QueryString(query);
    Parser parser(*queryContext, *ast, queryString);
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

  /// @brief start vertex pattern's collection/datasource label AST node
  static AstNode const* startVertexCollectionNode(ParsedMatch const& parsed) {
    EXPECT_NE(nullptr, parsed.matchNode);
    if (parsed.matchNode == nullptr || parsed.matchNode->numMembers() == 0) {
      return nullptr;
    }
    AstNode const* pattern = parsed.matchNode->getMember(0);
    EXPECT_EQ(NODE_TYPE_PATTERN_MATCH_EXPRESSION, pattern->type);
    if (pattern->type != NODE_TYPE_PATTERN_MATCH_EXPRESSION ||
        pattern->numMembers() == 0) {
      return nullptr;
    }
    AstNode const* start = pattern->getMember(0);
    // path variable may come first
    if (start->type == NODE_TYPE_PATTERN_PATH_VARIABLE &&
        pattern->numMembers() > 1) {
      start = pattern->getMember(1);
    }
    EXPECT_EQ(NODE_TYPE_PATTERN_NODE_PATTERN, start->type);
    if (start->type != NODE_TYPE_PATTERN_NODE_PATTERN) {
      return nullptr;
    }
    return start->getMember(1);
  }

  static NormalizedMatchStatement normalize(ParsedMatch const& parsed) {
    MatchPatternNormalizer normalizer(*parsed.ast);
    return normalizer.normalize(*parsed.matchNode);
  }

  static std::unique_ptr<ExecutionPlan> instantiatePlan(
      ParsedMatch const& parsed) {
    return ExecutionPlan::instantiateFromAst(parsed.ast.get(), false);
  }

  /// @brief run AST validation/optimization so expression trees match what
  /// MatchBuilder / MatchPatternNormalizer observe after Query::prepare.
  static void optimizeAst(ParsedMatch& parsed) {
    parsed.ast->validateAndOptimize(parsed.queryContext->trxForOptimization(),
                                    Ast::ValidateAndOptimizeOptions{});
  }

  /// @brief walk a NODE_TYPE_ATTRIBUTE_ACCESS chain into (variable, attrs).
  /// attrs[0] is the outermost attribute (closest to the reference).
  static void expectNestedAttributeAccess(
      AstNode const* node, std::string_view expectedVariable,
      std::vector<std::string_view> const& expectedAttributes) {
    ASSERT_NE(nullptr, node);
    ASSERT_FALSE(expectedAttributes.empty());

    std::vector<std::string> attrs;
    AstNode const* cur = node;
    while (cur->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      ast::AttributeAccessNode access(cur);
      attrs.emplace_back(access.getAttributeName());
      cur = access.getObject();
    }
    std::reverse(attrs.begin(), attrs.end());

    ASSERT_EQ(NODE_TYPE_REFERENCE, cur->type) << cur->getTypeString();
    ast::ReferenceNode ref(cur);
    ASSERT_NE(nullptr, ref.getVariable());
    EXPECT_EQ(expectedVariable, ref.getVariable()->name);

    ASSERT_EQ(expectedAttributes.size(), attrs.size());
    for (size_t i = 0; i < expectedAttributes.size(); ++i) {
      EXPECT_EQ(expectedAttributes[i], attrs[i]) << "attribute index " << i;
    }

    // Nested dotted access must not collapse into a single dotted name.
    if (expectedAttributes.size() > 1) {
      std::string collapsed;
      for (size_t i = 0; i < expectedAttributes.size(); ++i) {
        if (i != 0) {
          collapsed.push_back('.');
        }
        collapsed.append(expectedAttributes[i]);
      }
      EXPECT_FALSE(attrs.size() == 1 && attrs.front() == collapsed);
    }
  }

  /// @brief e["Data.Weight"] after parse (INDEXED_ACCESS) or after optimize
  /// (ATTRIBUTE_ACCESS with a single attribute name containing a dot).
  static void expectSingleDottedAttributeAccess(
      AstNode const* node, std::string_view expectedVariable,
      std::string_view expectedAttribute) {
    ASSERT_NE(nullptr, node);

    if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      ast::IndexedAccessNode indexed(node);
      AstNode const* object = indexed.getObject();
      AstNode const* index = indexed.getIndex();

      ASSERT_EQ(NODE_TYPE_REFERENCE, object->type) << object->getTypeString();
      ast::ReferenceNode ref(object);
      ASSERT_NE(nullptr, ref.getVariable());
      EXPECT_EQ(expectedVariable, ref.getVariable()->name);

      ASSERT_EQ(NODE_TYPE_VALUE, index->type) << index->getTypeString();
      ASSERT_TRUE(index->isStringValue());
      EXPECT_EQ(expectedAttribute, index->getStringView());
      return;
    }

    ASSERT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, node->type) << node->getTypeString();
    expectNestedAttributeAccess(node, expectedVariable, {expectedAttribute});
  }

  static AstNode const* equalityLhs(AstNode const* eqNode) {
    EXPECT_NE(nullptr, eqNode);
    if (eqNode == nullptr) {
      return nullptr;
    }
    EXPECT_EQ(NODE_TYPE_OPERATOR_BINARY_EQ, eqNode->type)
        << eqNode->getTypeString();
    if (eqNode->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
      return nullptr;
    }
    ast::RelationalOperatorNode rel(eqNode);
    return rel.getLeft();
  }

  static inline std::unique_ptr<mocks::MockRestAqlServer> server;
};

TEST_F(MatchPatternNormalizerTest, simpleVertex) {
  auto parsed = parseMatch("MATCH (v :vc) RETURN v");
  auto statement = normalize(parsed);

  ASSERT_EQ(1U, statement.patterns.size());
  auto const& pattern = statement.patterns.front();
  ASSERT_EQ(MatchPatternElement::Kind::kVertex, pattern.start.kind);
  ASSERT_TRUE(pattern.start.vertex.has_value());
  EXPECT_EQ("v", pattern.start.vertex->variable->name);
  EXPECT_EQ(MatchDataSource::Kind::kCollection,
            pattern.start.vertex->collection.kind());
  EXPECT_EQ("vc", pattern.start.vertex->collection.name());
  EXPECT_TRUE(pattern.start.vertex->properties.empty());
  EXPECT_FALSE(pattern.start.vertex->filter.has_value());
  EXPECT_FALSE(pattern.start.vertex->projection.has_value());
  EXPECT_TRUE(pattern.segments.empty());
}

TEST_F(MatchPatternNormalizerTest, outboundEdgeSingleCollection) {
  auto parsed = parseMatch("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN [v,e,w]");
  auto statement = normalize(parsed);

  ASSERT_EQ(1U, statement.patterns.size());
  auto const& pattern = statement.patterns.front();
  ASSERT_EQ(1U, pattern.segments.size());

  auto const& edge = pattern.segments.front().edge;
  EXPECT_EQ("e", edge.variable->name);
  ASSERT_EQ(1U, edge.collections.size());
  EXPECT_EQ("ec", edge.collections.front().name());
  EXPECT_EQ(MatchEdgeDirection::kOutbound, edge.direction);
  EXPECT_TRUE(edge.range.isDefaultFixedOne());
  EXPECT_TRUE(edge.range.isFixedOne());
  EXPECT_TRUE(edge.properties.empty());
  EXPECT_FALSE(edge.filter.has_value());
  EXPECT_FALSE(edge.projection.has_value());

  auto const& target = pattern.segments.front().target;
  ASSERT_EQ(MatchPatternElement::Kind::kVertex, target.kind);
  EXPECT_EQ("w", target.vertex->variable->name);
  EXPECT_EQ("vc", target.vertex->collection.name());
}

TEST_F(MatchPatternNormalizerTest, inboundEdge) {
  auto parsed = parseMatch("MATCH (v :vc) <-[ e :ec ]- (w :vc) RETURN [v,e,w]");
  auto statement = normalize(parsed);
  EXPECT_EQ(MatchEdgeDirection::kInbound,
            statement.patterns.front().segments.front().edge.direction);
}

TEST_F(MatchPatternNormalizerTest, anyDirectionEdge) {
  auto parsed = parseMatch("MATCH (v :vc) -[ e :ec ]- (w :vc) RETURN [v,e,w]");
  auto statement = normalize(parsed);
  EXPECT_EQ(MatchEdgeDirection::kAny,
            statement.patterns.front().segments.front().edge.direction);
}

TEST_F(MatchPatternNormalizerTest, multipleEdgeCollections) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc) RETURN [v,e,w]");
  auto statement = normalize(parsed);

  auto const& collections =
      statement.patterns.front().segments.front().edge.collections;
  ASSERT_EQ(2U, collections.size());
  EXPECT_EQ("ec", collections[0].name());
  EXPECT_EQ("ec2", collections[1].name());
}

TEST_F(MatchPatternNormalizerTest, literalCollectionVertexAndEdge) {
  auto parsed = parseMatch("MATCH (v :vc1) -[ e :ec1 ]-> (w :vc2) RETURN 1");

  // Literal collection labels are NODE_TYPE_COLLECTION with Ast-owned
  // strings (lexer registers via Ast::resources().registerString).
  AstNode const* vertexLabel = startVertexCollectionNode(parsed);
  ASSERT_NE(nullptr, vertexLabel);
  ASSERT_EQ(NODE_TYPE_COLLECTION, vertexLabel->type);
  EXPECT_EQ("vc1", vertexLabel->getStringView());

  auto statement = normalize(parsed);

  EXPECT_EQ("vc1", statement.patterns.front().start.vertex->collection.name());
  EXPECT_EQ("vc2", statement.patterns.front()
                       .segments.front()
                       .target.vertex->collection.name());
  EXPECT_EQ("ec1", statement.patterns.front()
                       .segments.front()
                       .edge.collections.front()
                       .name());
}

TEST_F(MatchPatternNormalizerTest, collectionBindParameter) {
  auto parsed = parseMatch("MATCH (v :@@vc) -[ e :@@ec ]-> (w :@@vc) RETURN 1");

  AstNode const* vertexLabel = startVertexCollectionNode(parsed);
  ASSERT_NE(nullptr, vertexLabel);
  ASSERT_EQ(NODE_TYPE_PARAMETER_DATASOURCE, vertexLabel->type);
  EXPECT_EQ("@vc", vertexLabel->getStringView());

  auto statement = normalize(parsed);

  EXPECT_EQ(MatchDataSource::Kind::kBindParameter,
            statement.patterns.front().start.vertex->collection.kind());
  EXPECT_EQ("@vc", statement.patterns.front().start.vertex->collection.name());

  auto const& edgeCollection =
      statement.patterns.front().segments.front().edge.collections.front();
  EXPECT_EQ(MatchDataSource::Kind::kBindParameter, edgeCollection.kind());
  EXPECT_EQ("@ec", edgeCollection.name());
}

TEST_F(MatchPatternNormalizerTest, resolvedCollectionBindParameter) {
  auto parsed =
      parseMatch("MATCH (v :@@vc) -[ e :@@ec ]-> (w :@@vc) RETURN 1", true,
                 {{"@vc", "resolved_vc"}, {"@ec", "resolved_ec"}});

  // After bind-parameter injection, @@ labels become NODE_TYPE_COLLECTION.
  // Collection-name string_views point into BindParameters' VPack Builder,
  // which ParsedMatch keeps alive (mirrors Query::_bindParameters).
  ASSERT_NE(nullptr, parsed.bindParameters);
  AstNode const* vertexLabel = startVertexCollectionNode(parsed);
  ASSERT_NE(nullptr, vertexLabel);
  ASSERT_EQ(NODE_TYPE_COLLECTION, vertexLabel->type)
      << vertexLabel->getTypeString();
  EXPECT_EQ("resolved_vc", vertexLabel->getStringView());

  AstNode const* edge =
      parsed.matchNode->getMember(0)->getMember(1)->getMember(0);
  ASSERT_EQ(NODE_TYPE_PATTERN_EDGE, edge->type);
  AstNode const* edgeCollections = edge->getMember(1);
  ASSERT_EQ(NODE_TYPE_ARRAY, edgeCollections->type);
  ASSERT_EQ(1U, edgeCollections->numMembers());
  AstNode const* edgeLabel = edgeCollections->getMember(0);
  ASSERT_EQ(NODE_TYPE_COLLECTION, edgeLabel->type);
  EXPECT_EQ("resolved_ec", edgeLabel->getStringView());

  auto statement = normalize(parsed);

  EXPECT_EQ(MatchDataSource::Kind::kCollection,
            statement.patterns.front().start.vertex->collection.kind());
  EXPECT_EQ("resolved_vc",
            statement.patterns.front().start.vertex->collection.name());
  EXPECT_EQ("resolved_ec", statement.patterns.front()
                               .segments.front()
                               .edge.collections.front()
                               .name());
  EXPECT_EQ(MatchDataSource::Kind::kCollection,
            statement.patterns.front()
                .segments.front()
                .target.vertex->collection.kind());
  EXPECT_EQ("resolved_vc", statement.patterns.front()
                               .segments.front()
                               .target.vertex->collection.name());
}

TEST_F(MatchPatternNormalizerTest, fixedRange) {
  auto parsed = parseMatch("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;
  EXPECT_TRUE(range.isDefaultFixedOne());
  EXPECT_TRUE(range.isFixedOne());
  EXPECT_EQ(1U, range.minDepth());
  EXPECT_TRUE(range.hasMaxDepth());
  EXPECT_EQ(1U, range.maxDepth());
}

TEST_F(MatchPatternNormalizerTest, fixedRangeThree) {
  auto parsed = parseMatch("MATCH (v :vc) -[ e :ec *3..3 ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;

  EXPECT_TRUE(range.isFixed());
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_FALSE(range.isFixedOne());
  EXPECT_EQ(3U, range.minDepth());
  EXPECT_TRUE(range.hasMaxDepth());
  EXPECT_EQ(3U, range.maxDepth());
}

TEST_F(MatchPatternNormalizerTest, boundedRange) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 2..5 ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_FALSE(range.isFixedOne());
  EXPECT_EQ(2U, range.minDepth());
  EXPECT_TRUE(range.hasMaxDepth());
  EXPECT_EQ(5U, range.maxDepth());
}

TEST_F(MatchPatternNormalizerTest, explicitFixedRangeIsNotDefault) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 1..1 ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;
  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_TRUE(range.isFixedOne());
  EXPECT_EQ(MatchPathRange::Kind::kBounded, range.kind());
}

TEST_F(MatchPatternNormalizerTest, unboundedRangeSemanticType) {
  auto range = MatchPathRange::unboundedMin(3);
  EXPECT_EQ(3U, range.minDepth());
  EXPECT_FALSE(range.hasMaxDepth());
  EXPECT_FALSE(range.isFixedOne());
  EXPECT_FALSE(range.isDefaultFixedOne());
}

TEST_F(MatchPatternNormalizerTest, projectionKeepPath) {
  auto parsed =
      parseMatch("MATCH (v :vc RETURN i) -[ e :ec ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& projection = statement.patterns.front().start.vertex->projection;
  ASSERT_TRUE(projection.has_value());
  ASSERT_EQ(1U, projection->items.size());
  EXPECT_EQ(MatchProjectionItem::Kind::kKeepAttribute,
            projection->items.front().kind);
  EXPECT_EQ("i", projection->items.front().name);
  ASSERT_EQ((std::vector<std::string>{"i"}), projection->items.front().path);
}

TEST_F(MatchPatternNormalizerTest, projectionAliasAndNestedPath) {
  auto parsed = parseMatch(
      "MATCH (v :vc RETURN idx = v.profile.first_name, status) "
      "-[ e :ec RETURN edgeI = e.i ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& vertexProjection =
      statement.patterns.front().start.vertex->projection;
  ASSERT_TRUE(vertexProjection.has_value());
  ASSERT_EQ(2U, vertexProjection->items.size());

  EXPECT_EQ(MatchProjectionItem::Kind::kAlias, vertexProjection->items[0].kind);
  EXPECT_EQ("idx", vertexProjection->items[0].name);
  ASSERT_NE(nullptr, vertexProjection->items[0].expression.node);

  EXPECT_EQ(MatchProjectionItem::Kind::kKeepAttribute,
            vertexProjection->items[1].kind);
  EXPECT_EQ("status", vertexProjection->items[1].name);
  ASSERT_EQ((std::vector<std::string>{"status"}),
            vertexProjection->items[1].path);

  auto const& edgeProjection =
      statement.patterns.front().segments.front().edge.projection;
  ASSERT_TRUE(edgeProjection.has_value());
  ASSERT_EQ(1U, edgeProjection->items.size());
  EXPECT_EQ(MatchProjectionItem::Kind::kAlias, edgeProjection->items[0].kind);
  EXPECT_EQ("edgeI", edgeProjection->items[0].name);
}

TEST_F(MatchPatternNormalizerTest, projectionNestedKeepPath) {
  auto parsed = parseMatch(
      "MATCH (v :vc RETURN profile.name) -[ e :ec ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& projection = statement.patterns.front().start.vertex->projection;
  ASSERT_TRUE(projection.has_value());
  ASSERT_EQ(1U, projection->items.size());
  EXPECT_EQ(MatchProjectionItem::Kind::kKeepAttribute,
            projection->items.front().kind);
  EXPECT_TRUE(projection->items.front().name.empty());
  ASSERT_EQ((std::vector<std::string>{"profile", "name"}),
            projection->items.front().path);
}

TEST_F(MatchPatternNormalizerTest, variableReferenceTarget) {
  auto parsed = parseMatch(
      "FOR w IN 1..1 LET start = \"vc/v0\" "
      "MATCH (v :vc) -[ e :ec ]-> (w) RETURN [v,e,w]");
  auto statement = normalize(parsed);

  auto const& target = statement.patterns.front().segments.front().target;
  ASSERT_EQ(MatchPatternElement::Kind::kVariableReference, target.kind);
  EXPECT_EQ("w", target.variableReference->name);
}

TEST_F(MatchPatternNormalizerTest, pathVariable) {
  auto parsed =
      parseMatch("MATCH p = (v :vc) -[ e :ec * 1..2 ]-> (w :vc) RETURN p");
  auto statement = normalize(parsed);

  ASSERT_NE(nullptr, statement.patterns.front().pathVariable);
  EXPECT_EQ("p", statement.patterns.front().pathVariable->name);
}

TEST_F(MatchPatternNormalizerTest, vertexPropertiesAndWhereFilter) {
  auto parsed = parseMatch(
      "MATCH (v :vc {j: 0, k: 1} WHERE v.i > 0) -[ e :ec {j: 2} WHERE e.i > 1 "
      "]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& vertex = statement.patterns.front().start.vertex;
  ASSERT_TRUE(vertex.has_value());
  ASSERT_EQ(2U, vertex->properties.size());
  EXPECT_EQ("j", vertex->properties[0].key);
  EXPECT_EQ("k", vertex->properties[1].key);
  ASSERT_TRUE(vertex->filter.has_value());
  ASSERT_NE(nullptr, vertex->filter->node);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_EQ(1U, edge.properties.size());
  EXPECT_EQ("j", edge.properties.front().key);
  ASSERT_TRUE(edge.filter.has_value());
}

TEST_F(MatchPatternNormalizerTest, vertexPropertiesAfterOptimize) {
  auto parsed = parseMatch("MATCH (v :vc {j: 0}) RETURN v");
  optimizeAst(parsed);

  AstNode const* vertexPattern = parsed.matchNode->getMember(0)->getMember(0);
  ASSERT_EQ(NODE_TYPE_PATTERN_NODE_PATTERN, vertexPattern->type);
  AstNode const* propsNode = vertexPattern->getMember(2);
  ASSERT_NE(nullptr, propsNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, propsNode->type) << propsNode->getTypeString();
  ASSERT_EQ(1U, propsNode->numMembers());
  AstNode const* propMember = propsNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT_ELEMENT, propMember->type);
  EXPECT_EQ("j", propMember->getStringView());

  auto statement = normalize(parsed);
  auto const& vertex = statement.patterns.front().start.vertex;
  ASSERT_TRUE(vertex.has_value());
  ASSERT_EQ(1U, vertex->properties.size());
  EXPECT_EQ("j", vertex->properties.front().key);
}

TEST_F(MatchPatternNormalizerTest, combinedPattern) {
  auto parsed = parseMatch(
      "MATCH p = (v :@@vc RETURN i) "
      "-[ e :@@ec1|@@ec2 * 2..4 ]-> (w :@@vc RETURN wId = w._key) "
      "RETURN p",
      true, {{"@vc", "mvc"}, {"@ec1", "mec1"}, {"@ec2", "mec2"}});
  auto statement = normalize(parsed);

  ASSERT_EQ(1U, statement.patterns.size());
  auto const& pattern = statement.patterns.front();
  EXPECT_EQ("p", pattern.pathVariable->name);
  EXPECT_EQ("v", pattern.start.vertex->variable->name);
  EXPECT_EQ("mvc", pattern.start.vertex->collection.name());
  ASSERT_TRUE(pattern.start.vertex->projection.has_value());

  ASSERT_EQ(1U, pattern.segments.size());
  auto const& segment = pattern.segments.front();
  ASSERT_EQ(2U, segment.edge.collections.size());
  EXPECT_EQ("mec1", segment.edge.collections[0].name());
  EXPECT_EQ("mec2", segment.edge.collections[1].name());
  EXPECT_EQ(MatchEdgeDirection::kOutbound, segment.edge.direction);
  EXPECT_EQ(2U, segment.edge.range.minDepth());
  EXPECT_EQ(4U, segment.edge.range.maxDepth());
  EXPECT_FALSE(segment.edge.projection.has_value());
  EXPECT_EQ("w", segment.target.vertex->variable->name);
  EXPECT_EQ("mvc", segment.target.vertex->collection.name());
  ASSERT_TRUE(segment.target.vertex->projection.has_value());
}

TEST_F(MatchPatternNormalizerTest, multiplePatternsInOneMatch) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec ]-> (w :vc), (a :vc2) -[ b :ec2 ]-> (c :vc2) "
      "RETURN 1");
  auto statement = normalize(parsed);

  ASSERT_EQ(2U, statement.patterns.size());
  EXPECT_EQ("v", statement.patterns[0].start.vertex->variable->name);
  EXPECT_EQ("a", statement.patterns[1].start.vertex->variable->name);
}

TEST_F(MatchPatternNormalizerTest, rejectsInvalidDirectionValue) {
  auto parsed = parseMatch("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN 1");

  AstNode* matchNode = const_cast<AstNode*>(parsed.matchNode);
  AstNode* edge = matchNode->getMember(0)->getMember(1)->getMember(0);
  edge->getMember(4)->setIntValue(99);

  MatchPatternNormalizer normalizer(*parsed.ast);
  EXPECT_THROW({ (void)normalizer.normalize(*parsed.matchNode); },
               basics::Exception);
}

TEST_F(MatchPatternNormalizerTest, rejectsInvalidRange) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 5..2 ]-> (w :vc) RETURN 1");
  MatchPatternNormalizer normalizer(*parsed.ast);
  EXPECT_THROW({ (void)normalizer.normalize(*parsed.matchNode); },
               basics::Exception);
}

TEST_F(MatchPatternNormalizerTest, whereNestedDottedAttributeAccess) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e.Data.Weight == 1 ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());
  AstNode const* lhs = equalityLhs(edge.filter->node);
  expectNestedAttributeAccess(lhs, "e", {"Data", "Weight"});
}

TEST_F(MatchPatternNormalizerTest, whereBracketDottedAttributeName) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e[\"Data.Weight\"] == 2 ]-> (w :vc) "
      "RETURN 1");
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());
  AstNode const* lhs = equalityLhs(edge.filter->node);
  // Pre-optimize: INDEXED_ACCESS with literal index "Data.Weight".
  expectSingleDottedAttributeAccess(lhs, "e", "Data.Weight");
  EXPECT_EQ(NODE_TYPE_INDEXED_ACCESS, lhs->type);
}

TEST_F(MatchPatternNormalizerTest, whereNestedAndBracketRemainDistinct) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e.Data.Weight == 1 AND "
      "e[\"Data.Weight\"] == 2 ]-> (w :vc) RETURN 1");
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());
  ASSERT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, edge.filter->node->type);

  ast::LogicalOperatorNode andNode(edge.filter->node);
  AstNode const* nestedLhs = equalityLhs(andNode.getLeft());
  AstNode const* dottedLhs = equalityLhs(andNode.getRight());

  expectNestedAttributeAccess(nestedLhs, "e", {"Data", "Weight"});
  expectSingleDottedAttributeAccess(dottedLhs, "e", "Data.Weight");

  // Distinct representations: nested ATTRIBUTE_ACCESS chain vs single
  // INDEXED_ACCESS / single ATTRIBUTE_ACCESS with a dotted name.
  EXPECT_NE(nestedLhs->type, dottedLhs->type);
  EXPECT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, nestedLhs->type);
  EXPECT_EQ(NODE_TYPE_INDEXED_ACCESS, dottedLhs->type);
}

TEST_F(MatchPatternNormalizerTest,
       whereNestedAndBracketRemainDistinctAfterOptimize) {
  // Mirrors Query prepare order: parse → validateAndOptimize → normalize
  // (as invoked from ExecutionPlan::fromNodeMatch).
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e.Data.Weight == 1 AND "
      "e[\"Data.Weight\"] == 2 ]-> (w :vc) RETURN 1");
  optimizeAst(parsed);
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());
  ASSERT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, edge.filter->node->type);

  ast::LogicalOperatorNode andNode(edge.filter->node);
  AstNode const* nestedLhs = equalityLhs(andNode.getLeft());
  AstNode const* dottedLhs = equalityLhs(andNode.getRight());

  // After optimizeIndexedAccess, e["Data.Weight"] becomes a single
  // ATTRIBUTE_ACCESS whose attribute name is the literal "Data.Weight".
  expectNestedAttributeAccess(nestedLhs, "e", {"Data", "Weight"});
  expectSingleDottedAttributeAccess(dottedLhs, "e", "Data.Weight");

  ASSERT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, nestedLhs->type);
  ASSERT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, dottedLhs->type);

  ast::AttributeAccessNode nestedLeaf(nestedLhs);
  ast::AttributeAccessNode dottedLeaf(dottedLhs);
  EXPECT_EQ("Weight", nestedLeaf.getAttributeName());
  EXPECT_EQ("Data.Weight", dottedLeaf.getAttributeName());
  EXPECT_NE(nestedLeaf.getAttributeName(), dottedLeaf.getAttributeName());

  // Nested form has Reference under Data; dotted form has Reference under
  // the single "Data.Weight" attribute.
  EXPECT_EQ(NODE_TYPE_ATTRIBUTE_ACCESS, nestedLeaf.getObject()->type);
  EXPECT_EQ(NODE_TYPE_REFERENCE, dottedLeaf.getObject()->type);
}

TEST_F(MatchPatternNormalizerTest, nestedAttributeEquivalentForms) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e.Data.Weight == 1 AND "
      "e[\"Data\"][\"Weight\"] == 1 AND e.Data[\"Weight\"] == 1 ]-> "
      "(w :vc) RETURN 1");
  optimizeAst(parsed);
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());

  // ((a AND b) AND c) after left-associative parsing.
  ASSERT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, edge.filter->node->type);
  ast::LogicalOperatorNode outer(edge.filter->node);
  ASSERT_EQ(NODE_TYPE_OPERATOR_BINARY_AND, outer.getLeft()->type);
  ast::LogicalOperatorNode inner(outer.getLeft());

  AstNode const* formDot = equalityLhs(inner.getLeft());
  AstNode const* formBrackets = equalityLhs(inner.getRight());
  AstNode const* formMixed = equalityLhs(outer.getRight());

  expectNestedAttributeAccess(formDot, "e", {"Data", "Weight"});
  expectNestedAttributeAccess(formBrackets, "e", {"Data", "Weight"});
  expectNestedAttributeAccess(formMixed, "e", {"Data", "Weight"});
}

TEST_F(MatchPatternNormalizerTest, edgePropertyDottedNameVsNestedAttribute) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec {nested: e.Data.Weight, dotted: "
      "e[\"Data.Weight\"]} ]-> "
      "(w :vc) RETURN 1");
  optimizeAst(parsed);
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_EQ(2U, edge.properties.size());

  EXPECT_EQ("nested", edge.properties[0].key);
  ASSERT_NE(nullptr, edge.properties[0].value.node);
  expectNestedAttributeAccess(edge.properties[0].value.node, "e",
                              {"Data", "Weight"});

  EXPECT_EQ("dotted", edge.properties[1].key);
  ASSERT_NE(nullptr, edge.properties[1].value.node);
  expectSingleDottedAttributeAccess(edge.properties[1].value.node, "e",
                                    "Data.Weight");
}

TEST_F(MatchPatternNormalizerTest, matchBuilderConsumesNormalizedSimpleVertex) {
  auto parsed = parseMatch("MATCH (v :vc) RETURN v");
  auto statement = normalize(parsed);

  ASSERT_EQ(1U, statement.patterns.size());
  ASSERT_TRUE(statement.patterns.front().start.vertex.has_value());
  EXPECT_EQ("vc", statement.patterns.front().start.vertex->collection.name());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest, matchBuilderConsumesNormalizedEdgeMatch) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec ]-> (w :vc) RETURN [v, e, w]");
  auto statement = normalize(parsed);

  ASSERT_EQ(1U, statement.patterns.front().segments.size());
  EXPECT_EQ("ec", statement.patterns.front()
                      .segments.front()
                      .edge.collections.front()
                      .name());
  EXPECT_TRUE(statement.patterns.front()
                  .segments.front()
                  .edge.range.isDefaultFixedOne());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       matchBuilderConsumesNormalizedMultipleEdgeCollections) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec|ec2 ]-> (w :vc) RETURN [v, e, w]");
  auto statement = normalize(parsed);

  ASSERT_EQ(
      2U, statement.patterns.front().segments.front().edge.collections.size());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       matchBuilderConsumesNormalizedFixedPathRange) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 2..2 ]-> (w :vc) RETURN [v, e, w]");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;

  EXPECT_FALSE(range.isDefaultFixedOne());
  EXPECT_TRUE(range.isFixed());
  EXPECT_EQ(2U, range.minDepth());
  EXPECT_EQ(2U, range.maxDepth());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       matchBuilderConsumesNormalizedBoundedPathRange) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[ e :ec * 1..3 ]-> (w :vc) RETURN [v, e, w]");
  auto statement = normalize(parsed);
  auto const& range = statement.patterns.front().segments.front().edge.range;

  EXPECT_EQ(1U, range.minDepth());
  EXPECT_TRUE(range.hasMaxDepth());
  EXPECT_EQ(3U, range.maxDepth());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       matchBuilderConsumesNormalizedPropertyAccessForms) {
  auto parsed = parseMatch(
      "MATCH (v :vc) -[ e :ec WHERE e.Data.Weight == 1 AND "
      "e[\"Data.Weight\"] == 2 ]-> (w :vc) RETURN e");
  optimizeAst(parsed);
  auto statement = normalize(parsed);

  auto const& edge = statement.patterns.front().segments.front().edge;
  ASSERT_TRUE(edge.filter.has_value());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       matchBuilderConsumesResolvedCollectionBindParameter) {
  auto parsed = parseMatch("MATCH (v :@@vc) -[ e :@@ec ]-> (w :@@vc) RETURN v",
                           true, {{"@vc", "vc"}, {"@ec", "ec"}});
  auto statement = normalize(parsed);

  EXPECT_EQ(MatchDataSource::Kind::kCollection,
            statement.patterns.front().start.vertex->collection.kind());
  EXPECT_EQ("vc", statement.patterns.front().start.vertex->collection.name());

  auto plan = instantiatePlan(parsed);
  ASSERT_NE(nullptr, plan);
}

TEST_F(MatchPatternNormalizerTest,
       fromNodeMatchRejectsUnresolvedCollectionBindParameter) {
  // Normalization intentionally preserves unresolved collection bind
  // parameters as MatchDataSource::Kind::kBindParameter. MatchBuilder then
  // rejects them via requireCollectionName (TRI_ERROR_INTERNAL).

  auto parsed = parseMatch("MATCH (v :@@vc) -[ e :ec ]-> (w :vc) RETURN 1");

  auto statement = normalize(parsed);
  ASSERT_EQ(1U, statement.patterns.size());
  ASSERT_TRUE(statement.patterns.front().start.vertex.has_value());
  EXPECT_EQ(MatchDataSource::Kind::kBindParameter,
            statement.patterns.front().start.vertex->collection.kind());

  try {
    // trackMemoryUsage=false: safer under gtest (see ExecutionPlan.h).
    auto plan = ExecutionPlan::instantiateFromAst(parsed.ast.get(), false);
    FAIL() << "expected unresolved collection bind parameter to throw during "
              "plan construction, but instantiateFromAst succeeded";
    (void)plan;
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_INTERNAL, ex.code());
  } catch (...) {
    FAIL() << "expected basics::Exception with TRI_ERROR_INTERNAL";
  }
}

TEST_F(MatchPatternNormalizerTest,
       fromNodeMatchRejectsUnresolvedEdgeCollectionBindParameter) {
  // Start/target use an existing collection so plan construction reaches
  // requireCollectionName on the unresolved edge bind parameter.
  auto parsed = parseMatch("MATCH (v :mvc) -[ e :@@ec ]-> (w :mvc) RETURN 1");

  auto statement = normalize(parsed);
  ASSERT_EQ(1U, statement.patterns.size());
  ASSERT_EQ(1U, statement.patterns.front().segments.size());
  EXPECT_EQ(MatchDataSource::Kind::kBindParameter, statement.patterns.front()
                                                       .segments.front()
                                                       .edge.collections.front()
                                                       .kind());

  try {
    auto plan = ExecutionPlan::instantiateFromAst(parsed.ast.get(), false);
    FAIL() << "expected unresolved edge collection bind parameter to throw "
              "during plan construction, but instantiateFromAst succeeded";
    (void)plan;
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_INTERNAL, ex.code());
  } catch (...) {
    FAIL() << "expected basics::Exception with TRI_ERROR_INTERNAL";
  }
}

}  // namespace
