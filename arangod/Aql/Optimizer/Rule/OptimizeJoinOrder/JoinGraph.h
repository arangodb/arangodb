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
#include <unordered_map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arangodb::aql {
class EnumerateCollectionNode;
class ExecutionNode;
class ExecutionPlan;
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
    /// cost estimator.
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

  auto nodeForVariable(Variable const* variable) -> Node*;

  void addNode(EnumerateCollectionNode* en);

  auto addJoinCondition(Variable const* v, AttributePath vAttributes,
                        Variable const* w, AttributePath wAttributes) -> Edge&;

  /// @brief a predicate that is neither an equijoin nor a constant
  /// restriction. Left in the plan; kept here only so the estimator can price
  /// it.
  void addResidual(AstNode const* node);

  /// @brief the edges incident to `node`. The reference is valid until the
  /// next addJoinCondition() call, which drops the adjacency index.
  auto getEdgesForNode(Node* node) -> std::vector<Edge*> const&;

  /// @brief partition the vertices into connected components. Order within
  /// and across components follows `nodes`, i.e. pointer order -- callers
  /// needing reproducibility must re-sort (see nodesInIdOrder).
  [[nodiscard]] auto connectedComponents() const
      -> std::vector<std::vector<Variable const*>>;

  [[nodiscard]] auto hasJoin() const noexcept -> bool { return !edges.empty(); }

  std::map<Variable const*, Node> nodes;
  std::vector<Edge> edges;
  std::vector<AstNode const*> residuals;

  /// @brief true when the run contains a calculation whose expression is not
  /// deterministic. Hoisting enumerations above such a calculation changes how
  /// many times it is evaluated, which changes results, so the run must not be
  /// reordered.
  bool hasNonDeterministicCalculation = false;

 private:
  auto ensureEdge(Variable const* v, Variable const* w) -> Edge&;

  /// @brief incident edges per vertex, built on first use. Holds `Edge*` into
  /// `edges`, which appending reallocates, so ensureEdge() drops it whenever
  /// it adds an edge; the graph is fully built before the search reads it.
  void buildAdjacency();

  std::unordered_map<Node const*, std::vector<Edge*>> _adjacency;
  bool _adjacencyBuilt = false;
};

/// @brief build the join graph for the maximal run of adjacent collection
/// enumerations that starts at `firstEnumeration`, following parents towards
/// the plan root. Returns the graph and, via `next`, the first node that
/// terminated the run (nullptr if the run reached the root).
auto buildJoinGraph(ExecutionPlan const* plan, ExecutionNode* firstEnumeration,
                    ExecutionNode*& next) -> JoinGraph;

}  // namespace arangodb::aql
