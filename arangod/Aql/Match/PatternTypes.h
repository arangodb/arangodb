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

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb::aql {
struct AstNode;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief references an expression subtree owned by the query Ast
/// valid for the lifetime of the associated Ast object
struct ExpressionRef {
  AstNode const* node{nullptr};
};

/// @brief a collection name or an unresolved collection bind parameter
struct DataSource {
  enum class Kind : uint8_t { kCollection, kBindParameter };

  DataSource() noexcept = default;

  static DataSource collection(std::string name);
  static DataSource bindParameter(std::string name);

  Kind kind() const noexcept { return _kind; }
  std::string_view name() const noexcept { return _name; }

 private:
  DataSource(Kind kind, std::string name);

  Kind _kind{Kind::kCollection};
  std::string _name;
};

enum class EdgeDirection : uint8_t { kOutbound, kInbound, kAny };

/// @brief path depth for a MATCH edge pattern.
/// Distinguishes the default single-hop form (no `*`) from an explicit
/// `* min..max` range, including `* 1..1`, because planning treats them
/// differently (enumerate vs path-producing traversal).
struct PathRange {
  enum class Kind : uint8_t { kDefaultFixedOne, kBounded, kUnboundedMin };

  PathRange() noexcept = default;

  /// @brief omitted range (no `*`): plan as fixed one-hop edge access
  static PathRange defaultFixedOne();
  static PathRange bounded(uint64_t minDepth, uint64_t maxDepth);
  static PathRange unboundedMin(uint64_t minDepth);

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
  PathRange(Kind kind, uint64_t minDepth, std::optional<uint64_t> maxDepth);

  Kind _kind{Kind::kDefaultFixedOne};
  uint64_t _minDepth{1};
  std::optional<uint64_t> _maxDepth{1};
};

struct PropertyConstraint {
  std::string key;
  ExpressionRef value;
};

/// @brief Attributes always present in a projected MATCH vertex document.
inline constexpr std::array<std::string_view, 1>
    kMandatoryDocumentProjectionAttributes{"_id"};

/// @brief Attributes always present in a projected MATCH edge document.
inline constexpr std::array<std::string_view, 3>
    kMandatoryEdgeDocumentProjectionAttributes{"_id", "_from", "_to"};

/// @brief One RETURN item from an in-pattern MATCH projection.
///
/// All strings are owned. Alias expression subtrees remain Ast-owned via
/// ExpressionRef (same lifetime model as filters/properties).
struct ProjectionItem {
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
  ExpressionRef expression;

  [[nodiscard]] static ProjectionItem keepPath(std::vector<std::string> path);
  [[nodiscard]] static ProjectionItem keepLiteral(std::string key);
  [[nodiscard]] static ProjectionItem alias(std::string name,
                                            ExpressionRef expression);

  [[nodiscard]] bool isKeep() const noexcept {
    return kind == Kind::kKeepAttribute || kind == Kind::kKeepLiteral;
  }
  [[nodiscard]] bool isAlias() const noexcept { return kind == Kind::kAlias; }

  /// @brief Top-level output object key used for collision / reserved checks
  [[nodiscard]] std::string_view topLevelKey() const noexcept;
};

/// @brief Stable semantic representation of an in-pattern MATCH projection.
/// Independent of parser positional AST layout.
struct Projection {
  std::vector<ProjectionItem> items;
};

struct NormalizedVertex {
  Variable const* variable{nullptr};
  DataSource collection;
  std::vector<PropertyConstraint> properties;
  std::optional<ExpressionRef> filter;
  std::optional<Projection> projection;
};

struct NormalizedEdge {
  Variable const* variable{nullptr};
  std::vector<DataSource> collections;
  /// @brief parser-owned collection datasource nodes (member 1 of
  /// PATTERN_EDGE). Used by Builder when constructing traversal collection
  /// lists so collection nodes match parser registration/lifetime semantics.
  std::vector<AstNode const*> collectionAstNodes;
  std::vector<PropertyConstraint> properties;
  std::optional<ExpressionRef> filter;
  EdgeDirection direction{EdgeDirection::kOutbound};
  PathRange range{PathRange::defaultFixedOne()};
  std::optional<Projection> projection;
};

struct PatternElement {
  enum class Kind : uint8_t { kVertex, kVariableReference };

  Kind kind{Kind::kVariableReference};
  std::optional<NormalizedVertex> vertex;
  Variable const* variableReference{nullptr};
};

struct NormalizedSegment {
  NormalizedEdge edge;
  PatternElement target;
};

struct NormalizedPattern {
  Variable const* pathVariable{nullptr};
  PatternElement start;
  std::vector<NormalizedSegment> segments;
};

struct NormalizedStatement {
  std::vector<NormalizedPattern> patterns;
};

}  // namespace arangodb::aql::match
