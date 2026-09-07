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

#include "Aql/MatchPatternTypes.h"

namespace arangodb::aql {

class Ast;
struct AstNode;

/// @brief converts MATCH parser AST into a semantic representation
class MatchPatternNormalizer {
 public:
  explicit MatchPatternNormalizer(Ast& ast) noexcept;

  [[nodiscard]] NormalizedMatchStatement normalize(
      AstNode const& matchNode) const;

 private:
  [[nodiscard]] NormalizedMatchPattern normalizePattern(
      AstNode const& matchExpr) const;
  [[nodiscard]] MatchPatternElement normalizeStartElement(
      AstNode const& node) const;
  [[nodiscard]] NormalizedMatchSegment normalizeSegment(
      AstNode const& segment) const;
  [[nodiscard]] NormalizedVertex normalizeVertex(
      AstNode const& nodePattern) const;
  [[nodiscard]] NormalizedEdge normalizeEdge(AstNode const& edge) const;

  [[nodiscard]] MatchDataSource normalizeDataSource(AstNode const& node) const;
  [[nodiscard]] std::vector<MatchDataSource> normalizeDataSourceList(
      AstNode const* node) const;
  [[nodiscard]] std::vector<MatchPropertyConstraint> normalizeProperties(
      AstNode const* node) const;
  [[nodiscard]] std::optional<MatchExpressionRef> normalizeFilter(
      AstNode const* node) const;
  [[nodiscard]] std::optional<MatchProjection> normalizeProjection(
      AstNode const* node) const;
  [[nodiscard]] MatchEdgeDirection normalizeDirection(
      AstNode const* node) const;
  [[nodiscard]] MatchPathRange normalizeRange(AstNode const* node) const;

  Ast& _ast;
};

}  // namespace arangodb::aql
