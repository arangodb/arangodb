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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb::aql {

struct AstNode;
struct Variable;

/// @brief references an expression subtree owned by the query Ast
/// valid for the lifetime of the associated Ast object
struct MatchExpressionRef {
  AstNode const* node{nullptr};
};

/// @brief a collection name or an unresolved collection bind parameter
struct MatchDataSource {
  enum class Kind : uint8_t { kCollection, kBindParameter };

  MatchDataSource() noexcept = default;

  static MatchDataSource collection(std::string name);
  static MatchDataSource bindParameter(std::string name);

  Kind kind() const noexcept { return _kind; }
  std::string_view name() const noexcept { return _name; }

 private:
  MatchDataSource(Kind kind, std::string name);

  Kind _kind{Kind::kCollection};
  std::string _name;
};

enum class MatchEdgeDirection : uint8_t { kOutbound, kInbound, kAny };

/// @brief path depth for a MATCH edge pattern.
/// Distinguishes the default single-hop form (no `*`) from an explicit
/// `* min..max` range, including `* 1..1`, because planning treats them
/// differently (enumerate vs path-producing traversal).
struct MatchPathRange {
  enum class Kind : uint8_t { kDefaultFixedOne, kBounded, kUnboundedMin };

  MatchPathRange() noexcept = default;

  /// @brief omitted range (no `*`): plan as fixed one-hop edge access
  static MatchPathRange defaultFixedOne();
  static MatchPathRange bounded(uint64_t minDepth, uint64_t maxDepth);
  static MatchPathRange unboundedMin(uint64_t minDepth);

  Kind kind() const noexcept { return _kind; }
  /// @brief true when the pattern had no `*` (default one-hop)
  bool isDefaultFixedOne() const noexcept {
    return _kind == Kind::kDefaultFixedOne;
  }
  uint64_t minDepth() const noexcept { return _minDepth; }
  bool hasMaxDepth() const noexcept { return _maxDepth.has_value(); }
  uint64_t maxDepth() const noexcept { return *_maxDepth; }
  bool isFixedOne() const noexcept;
  bool isFixed() const noexcept;

 private:
  MatchPathRange(Kind kind, uint64_t minDepth,
                 std::optional<uint64_t> maxDepth);

  Kind _kind{Kind::kDefaultFixedOne};
  uint64_t _minDepth{1};
  std::optional<uint64_t> _maxDepth{1};
};

struct MatchPropertyConstraint {
  std::string key;
  MatchExpressionRef value;
};

/// @brief MATCH projection reserved attributes that are auto-injected into
/// projected objects and ignored when the user requests them.
///
/// This is intentionally narrower than document system attributes:
/// `_key` and `_rev` remain user-requestable keeps in MATCH projections.
enum class MatchProjectionReservedAttribute : uint8_t {
  kNone,
  kId,
  kFrom,
  kTo,
};

/// @brief Classify a top-level MATCH projection attribute name.
/// @param isEdge true when projecting an edge pattern variable
[[nodiscard]] MatchProjectionReservedAttribute
classifyMatchProjectionReservedAttribute(std::string_view name,
                                         bool isEdge) noexcept;

/// @brief true when @p name is reserved for MATCH projection auto-injection
[[nodiscard]] bool isMatchProjectionReservedAttribute(std::string_view name,
                                                      bool isEdge) noexcept;

/// @brief Attributes always present in a projected MATCH vertex/edge object.
/// Vertex: `_id`. Edge: `_id`, `_from`, `_to`.
[[nodiscard]] std::vector<std::string_view> mandatoryMatchProjectionAttributes(
    bool isEdge);

/// @brief One RETURN item from an in-pattern MATCH projection.
///
/// All strings are owned. Alias expression subtrees remain Ast-owned via
/// MatchExpressionRef (same lifetime model as filters/properties).
struct MatchProjectionItem {
  enum class Kind : uint8_t {
    /// Unquoted keep path. Nested dotted access is a multi-segment path:
    /// `profile.name` → path {"profile","name"}.
    kKeepAttribute,
    /// Quoted literal keep. Dots inside the quotes are NOT hierarchy:
    /// `"profile.name"` → path {"profile.name"}.
    kKeepLiteral,
    /// Alias / flatten: `name = <expression>` (expression is normal AQL scope).
    kAlias,
  };

  Kind kind{Kind::kKeepAttribute};
  /// @brief alias name for kAlias; for single-segment keeps equals path[0];
  /// empty for multi-segment keep paths
  std::string name;
  /// @brief keep attribute path segments. Empty for aliases.
  /// Quoted literal keeps are a single-element path whose value may contain
  /// dots that are NOT hierarchy.
  std::vector<std::string> path;
  /// @brief only set for alias items; Ast-owned expression subtree
  MatchExpressionRef expression;

  [[nodiscard]] static MatchProjectionItem keepPath(
      std::vector<std::string> path);
  [[nodiscard]] static MatchProjectionItem keepLiteral(std::string key);
  [[nodiscard]] static MatchProjectionItem alias(std::string name,
                                                 MatchExpressionRef expression);

  [[nodiscard]] bool isKeep() const noexcept {
    return kind == Kind::kKeepAttribute || kind == Kind::kKeepLiteral;
  }
  [[nodiscard]] bool isAlias() const noexcept { return kind == Kind::kAlias; }

  /// @brief Top-level output object key used for collision / reserved checks
  [[nodiscard]] std::string_view topLevelKey() const noexcept;
};

/// @brief Stable semantic representation of an in-pattern MATCH projection.
/// Independent of parser positional AST layout.
struct MatchProjection {
  std::vector<MatchProjectionItem> items;
};

struct NormalizedVertex {
  Variable const* variable{nullptr};
  MatchDataSource collection;
  std::vector<MatchPropertyConstraint> properties;
  std::optional<MatchExpressionRef> filter;
  std::optional<MatchProjection> projection;
};

struct NormalizedEdge {
  Variable const* variable{nullptr};
  std::vector<MatchDataSource> collections;
  /// @brief parser-owned collection datasource nodes (member 1 of
  /// PATTERN_EDGE). Used by MatchBuilder when constructing traversal collection
  /// lists so collection nodes match parser registration/lifetime semantics.
  std::vector<AstNode const*> collectionAstNodes;
  std::vector<MatchPropertyConstraint> properties;
  std::optional<MatchExpressionRef> filter;
  MatchEdgeDirection direction{MatchEdgeDirection::kOutbound};
  MatchPathRange range{MatchPathRange::defaultFixedOne()};
  std::optional<MatchProjection> projection;
};

struct MatchPatternElement {
  enum class Kind : uint8_t { kVertex, kVariableReference };

  Kind kind{Kind::kVariableReference};
  std::optional<NormalizedVertex> vertex;
  Variable const* variableReference{nullptr};

  Variable const* outputVariable() const noexcept;
};

struct NormalizedMatchSegment {
  NormalizedEdge edge;
  MatchPatternElement target;
};

struct NormalizedMatchPattern {
  Variable const* pathVariable{nullptr};
  MatchPatternElement start;
  std::vector<NormalizedMatchSegment> segments;
};

struct NormalizedMatchStatement {
  std::vector<NormalizedMatchPattern> patterns;
};

}  // namespace arangodb::aql
