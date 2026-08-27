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
  AstNode const* node;
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

struct MatchProjectionItem {
  enum class Kind : uint8_t { kKeepAttribute, kAlias };

  Kind kind;
  /// @brief alias name for kAlias; for single-segment keeps equals path[0]
  std::string name;
  /// @brief keep attribute path segments (COR-741 nested keeps). Empty for
  /// aliases. Quoted literal keeps are a single-element path whose value may
  /// contain dots that are NOT hierarchy.
  std::vector<std::string> path;
  /// @brief only set for alias items
  MatchExpressionRef expression;
};

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
  /// @brief parser-owned collection datasource nodes (member 1 of PATTERN_EDGE).
  /// Used by MatchBuilder when constructing traversal collection lists so
  /// collection nodes match parser registration/lifetime semantics.
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
