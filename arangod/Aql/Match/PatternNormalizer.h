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

#include "Aql/Match/PatternTypes.h"

namespace arangodb::aql {
class Ast;
struct AstNode;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief converts MATCH parser AST into a semantic representation
class PatternNormalizer {
 public:
  explicit PatternNormalizer(Ast& ast) noexcept;

  [[nodiscard]] NormalizedStatement normalize(
      AstNode const& matchNode) const;

 private:
  [[nodiscard]] NormalizedPattern normalizePattern(
      AstNode const& matchExpr) const;
  [[nodiscard]] PatternElement normalizeStartElement(
      AstNode const& node) const;
  [[nodiscard]] NormalizedSegment normalizeSegment(
      AstNode const& segment) const;
  [[nodiscard]] NormalizedVertex normalizeVertex(
      AstNode const& nodePattern) const;
  [[nodiscard]] NormalizedEdge normalizeEdge(AstNode const& edge) const;

  [[nodiscard]] DataSource normalizeDataSource(AstNode const& node) const;
  [[nodiscard]] std::vector<DataSource> normalizeDataSourceList(
      AstNode const* node) const;
  [[nodiscard]] std::vector<PropertyConstraint> normalizeProperties(
      AstNode const* node) const;
  [[nodiscard]] std::optional<ExpressionRef> normalizeFilter(
      AstNode const* node) const;
  [[nodiscard]] std::optional<Projection> normalizeProjection(
      AstNode const* node) const;
  [[nodiscard]] EdgeDirection normalizeDirection(
      AstNode const* node) const;
  [[nodiscard]] PathRange normalizeRange(AstNode const* node) const;

  Ast& _ast;
};

}  // namespace arangodb::aql::match
