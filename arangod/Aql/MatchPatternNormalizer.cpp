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

#include "MatchPatternNormalizer.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <cmath>
#include <cstdint>

namespace arangodb::aql {
namespace {

uint64_t checkDepthValue(AstNode const* node) {
  if (node == nullptr || !node->isNumericValue()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }
  double const v = node->getDoubleValue();
  if (v > static_cast<double>(INT64_MAX)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }

  double intpart;
  if (std::modf(v, &intpart) != 0.0 || v < 0.0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }
  return static_cast<uint64_t>(v);
}

}  // namespace

MatchPatternNormalizer::MatchPatternNormalizer(Ast& ast) noexcept : _ast{ast} {}

NormalizedMatchStatement MatchPatternNormalizer::normalize(
    AstNode const& matchNode) const {
  TRI_ASSERT(matchNode.type == NODE_TYPE_MATCH);

  NormalizedMatchStatement statement;
  statement.patterns.reserve(matchNode.numMembers());

  for (size_t i = 0; i < matchNode.numMembers(); ++i) {
    statement.patterns.push_back(
        normalizePattern(*matchNode.getMemberUnchecked(i)));
  }

  return statement;
}

NormalizedMatchPattern MatchPatternNormalizer::normalizePattern(
    AstNode const& matchExpr) const {
  TRI_ASSERT(matchExpr.type == NODE_TYPE_PATTERN_MATCH_EXPRESSION);

  NormalizedMatchPattern pattern;
  pattern.pathVariable = nullptr;
  bool hasStart = false;

  for (size_t i = 0; i < matchExpr.numMembers(); ++i) {
    AstNode const* member = matchExpr.getMemberUnchecked(i);

    switch (member->type) {
      case NODE_TYPE_PATTERN_PATH_VARIABLE:
        if (pattern.pathVariable != nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              "multiple path variables in match expression");
        }
        pattern.pathVariable = static_cast<Variable const*>(member->getData());
        break;

      case NODE_TYPE_PATTERN_NODE_PATTERN:
      case NODE_TYPE_REFERENCE:
        if (hasStart) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "multiple start nodes in match "
                                         "expression");
        }
        pattern.start = normalizeStartElement(*member);
        hasStart = true;
        break;

      case NODE_TYPE_PATTERN_SEGMENT:
        pattern.segments.push_back(normalizeSegment(*member));
        break;

      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected match expression member");
    }
  }

  if (!hasStart) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "match expression without start node");
  }

  return pattern;
}

MatchPatternElement MatchPatternNormalizer::normalizeStartElement(
    AstNode const& node) const {
  if (node.type == NODE_TYPE_REFERENCE) {
    MatchPatternElement element;
    element.kind = MatchPatternElement::Kind::kVariableReference;
    element.variableReference = static_cast<Variable const*>(node.getData());
    return element;
  }

  TRI_ASSERT(node.type == NODE_TYPE_PATTERN_NODE_PATTERN);
  MatchPatternElement element;
  element.kind = MatchPatternElement::Kind::kVertex;
  element.vertex = normalizeVertex(node);
  return element;
}

NormalizedMatchSegment MatchPatternNormalizer::normalizeSegment(
    AstNode const& segment) const {
  ast::PatternSegment typed{&segment};

  NormalizedMatchSegment result;
  result.edge = normalizeEdge(*typed.getEdge().get());
  result.target = normalizeStartElement(*typed.getNode());
  return result;
}

NormalizedVertex MatchPatternNormalizer::normalizeVertex(
    AstNode const& nodePattern) const {
  ast::PatternNodePattern typed{&nodePattern};

  NormalizedVertex vertex;
  vertex.variable = normalizeOutputVariable(typed.getOutVariable());

  AstNode const* label = typed.getLabels();
  if (label == nullptr || label->type == NODE_TYPE_VALUE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "match vertex without collection label");
  }
  vertex.collection = normalizeDataSource(*label);
  vertex.properties = normalizeProperties(typed.getProperties());
  vertex.filter = normalizeFilter(typed.getFilter());
  vertex.projection = normalizeProjection(typed.getProjection());
  return vertex;
}

NormalizedEdge MatchPatternNormalizer::normalizeEdge(
    AstNode const& edge) const {
  ast::PatternEdge typed{&edge};

  NormalizedEdge result;
  result.variable = normalizeOutputVariable(typed.getOutVariable());
  AstNode const* collectionsNode = typed.getCollections();
  result.collections = normalizeDataSourceList(collectionsNode);
  if (collectionsNode != nullptr && collectionsNode->type == NODE_TYPE_ARRAY) {
    result.collectionAstNodes.reserve(collectionsNode->numMembers());
    for (size_t i = 0; i < collectionsNode->numMembers(); ++i) {
      result.collectionAstNodes.push_back(collectionsNode->getMember(i));
    }
  }
  if (result.collections.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "match edge without collection label");
  }
  result.properties = normalizeProperties(typed.getProperties());
  result.filter = normalizeFilter(typed.getFilter());
  result.direction = normalizeDirection(typed.getDirection());
  result.range = normalizeRange(typed.getRange());
  result.projection = normalizeProjection(typed.getProjection());
  return result;
}

MatchDataSource MatchPatternNormalizer::normalizeDataSource(
    AstNode const& node) const {
  switch (node.type) {
    case NODE_TYPE_COLLECTION:
      return MatchDataSource::collection(std::string(node.getStringView()));
    case NODE_TYPE_PARAMETER_DATASOURCE:
      return MatchDataSource::bindParameter(std::string(node.getStringView()));
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "unexpected data source node in match pattern normalization");
  }
}

std::vector<MatchDataSource> MatchPatternNormalizer::normalizeDataSourceList(
    AstNode const* node) const {
  std::vector<MatchDataSource> collections;
  if (node == nullptr || node->type == NODE_TYPE_VALUE) {
    return collections;
  }

  TRI_ASSERT(node->type == NODE_TYPE_ARRAY);
  collections.reserve(node->numMembers());
  for (size_t i = 0; i < node->numMembers(); ++i) {
    collections.push_back(normalizeDataSource(*node->getMember(i)));
  }
  return collections;
}

std::vector<MatchPropertyConstraint>
MatchPatternNormalizer::normalizeProperties(AstNode const* node) const {
  std::vector<MatchPropertyConstraint> properties;
  if (node == nullptr || node->type == NODE_TYPE_NOP) {
    return properties;
  }

  TRI_ASSERT(node->type == NODE_TYPE_OBJECT);
  properties.reserve(node->numMembers());
  for (size_t i = 0; i < node->numMembers(); ++i) {
    AstNode const* member = node->getMember(i);
    TRI_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);
    properties.push_back(MatchPropertyConstraint{
        std::string(member->getStringView()), {member->getMember(0)}});
  }
  return properties;
}

std::optional<MatchExpressionRef> MatchPatternNormalizer::normalizeFilter(
    AstNode const* node) const {
  if (node == nullptr || node->type == NODE_TYPE_NOP) {
    return std::nullopt;
  }
  return MatchExpressionRef{node};
}

std::optional<MatchProjection> MatchPatternNormalizer::normalizeProjection(
    AstNode const* node) const {
  if (node == nullptr || node->type == NODE_TYPE_NOP) {
    return std::nullopt;
  }

  TRI_ASSERT(node->type == NODE_TYPE_ARRAY);
  MatchProjection projection;
  projection.items.reserve(node->numMembers());

  for (size_t i = 0; i < node->numMembers(); ++i) {
    AstNode const* item = node->getMemberUnchecked(i);
    if (item->type == NODE_TYPE_OBJECT_ELEMENT) {
      projection.items.push_back(
          MatchProjectionItem{MatchProjectionItem::Kind::kAlias,
                              std::string(item->getStringView()),
                              {},
                              {item->getMember(0)}});
    } else if (item->type == NODE_TYPE_ARRAY) {
      // Unquoted keep path: ARRAY of path segments (nested when size > 1).
      std::vector<std::string> path;
      path.reserve(item->numMembers());
      for (size_t j = 0; j < item->numMembers(); ++j) {
        AstNode const* part = item->getMemberUnchecked(j);
        TRI_ASSERT(part->isStringValue());
        path.emplace_back(part->getString());
      }
      TRI_ASSERT(!path.empty());
      std::string name = path.size() == 1 ? path.front() : std::string{};
      projection.items.push_back(
          MatchProjectionItem{MatchProjectionItem::Kind::kKeepAttribute,
                              std::move(name),
                              std::move(path),
                              {}});
    } else if (item->type == NODE_TYPE_VALUE && item->isStringValue()) {
      // Quoted literal keep: single top-level key (dots are not hierarchy).
      std::string name{item->getStringView()};
      std::vector<std::string> path{name};
      projection.items.push_back(
          MatchProjectionItem{MatchProjectionItem::Kind::kKeepAttribute,
                              std::move(name),
                              std::move(path),
                              {}});
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unexpected match projection item");
    }
  }

  return projection;
}

MatchEdgeDirection MatchPatternNormalizer::normalizeDirection(
    AstNode const* node) const {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_VALUE);

  switch (node->getIntValue()) {
    case 1:
      return MatchEdgeDirection::kInbound;
    case 2:
      return MatchEdgeDirection::kOutbound;
    case 3:
      return MatchEdgeDirection::kAny;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid direction for match expression");
  }
}

MatchPathRange MatchPatternNormalizer::normalizeRange(
    AstNode const* node) const {
  if (node == nullptr || node->type == NODE_TYPE_NOP) {
    return MatchPathRange::defaultFixedOne();
  }

  TRI_ASSERT(node->type == NODE_TYPE_RANGE);
  TRI_ASSERT(node->numMembers() == 2);

  uint64_t const minDepth = checkDepthValue(node->getMember(0));
  uint64_t const maxDepth = checkDepthValue(node->getMember(1));
  if (maxDepth < minDepth) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }
  return MatchPathRange::bounded(minDepth, maxDepth);
}

Variable const* MatchPatternNormalizer::normalizeOutputVariable(
    AstNode const* node) const {
  if (node == nullptr || node->type == NODE_TYPE_VALUE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "match pattern without output variable");
  }

  if (node->type == NODE_TYPE_REFERENCE || node->type == NODE_TYPE_VARIABLE) {
    return static_cast<Variable const*>(node->getData());
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "unexpected output variable node in match pattern normalization");
}

}  // namespace arangodb::aql
