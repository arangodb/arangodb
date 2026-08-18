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

#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arangodb::aql {
class EnumerateCollectionNode;
struct AstNode;
struct Variable;

/// @brief a path of nested attribute accesses, e.g. `doc.a.b` -> {"a", "b"}.
using AttributePath = std::vector<std::string_view>;

/// @brief if `n` is a chain of attribute accesses rooted at a variable
/// reference, return that variable and the path. A leading `_id` is rewritten
/// to `_key` so it matches the primary index.
auto extractAttributeAccess(AstNode const* n)
    -> std::optional<std::pair<Variable const*, AttributePath>>;

/// @brief The join graph describes a maximal run of adjacent collection
/// enumerations (`FOR`-loops) that are connected via equijoin conditions.
struct JoinGraph {
  struct Node {
    EnumerateCollectionNode* executionNode;

    /// @brief attribute paths that are constrained by a constant restriction
    /// on this node (e.g. the `x` of `doc.x == 'foo'`).
    std::vector<AttributePath> conditions;

    /// @brief residual predicates that reference this node's variable and no
    /// other graph variable. Priced with a default selectivity factor by the
    /// cost estimator; see the design doc's residual-selectivity section.
    std::vector<AstNode const*> residuals;

    explicit Node(EnumerateCollectionNode* executionNode)
        : executionNode(executionNode) {}
  };

  struct Edge {
    Node* from;
    Node* to;

    std::vector<AttributePath> fromAttributes;
    std::vector<AttributePath> toAttributes;

    Edge(Node* from, Node* to) : from(from), to(to) {}
  };

  /// @brief the vertex for a given out variable, or nullptr if the variable is
  /// not part of this join graph.
  auto nodeForVariable(Variable const* variable) -> Node*;

  void addNode(EnumerateCollectionNode* en);

  /// @brief record an equijoin condition `v.<vAttributes> == w.<wAttributes>`
  /// as an edge between the vertices for `v` and `w`.
  auto addJoinCondition(Variable const* v, AttributePath vAttributes,
                        Variable const* w, AttributePath wAttributes) -> Edge&;

  /// @brief record a predicate that could not be classified as an equijoin or a
  /// constant restriction. It is left untouched in the plan and only kept here
  /// for later optimizer stages.
  void addResidual(AstNode const* node);

  auto getEdgesForNode(Node* node) -> std::vector<Edge*>;

  /// @brief partition the vertices into connected components (the graph may be
  /// disconnected). Each returned vector holds the out variables of one
  /// component. Order within and across components is unspecified.
  [[nodiscard]] auto connectedComponents() const
      -> std::vector<std::vector<Variable const*>>;

  /// @brief true if this graph describes an actual join, i.e. it has at least
  /// one equijoin edge.
  [[nodiscard]] auto hasJoin() const noexcept -> bool { return !edges.empty(); }

  std::map<Variable const*, Node> nodes;
  std::vector<Edge> edges;
  std::vector<AstNode const*> residuals;

 private:
  auto ensureEdge(Variable const* v, Variable const* w) -> Edge&;
};

}  // namespace arangodb::aql
