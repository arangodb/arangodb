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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"

#include <span>

namespace arangodb::aql {

/// @brief the result of a distinct-tuple lookup.
struct DistinctEstimate {
  /// @brief |C_S| : distinct *tuples* over the attribute set S, i.e. for
  /// S = {x,y} the distinct (x,y) combinations. A count, not a ratio.
  double value = 1.0;
  /// @brief true when no index could supply an estimate and `value` is the
  /// fallback of 1.
  bool defaulted = true;
};

/// @brief statistics about the data a join graph reads. Implementations must
/// depend only on the abstract Index and Collection interfaces, never on a
/// concrete index class or a particular storage engine.
class JoinStatistics {
 public:
  virtual ~JoinStatistics() = default;

  /// @brief |C| : documents in the node's collection.
  [[nodiscard]] virtual auto documentCount(JoinGraph::Node const& node) const
      -> double = 0;

  /// @brief |C_S| : distinct tuples over the attribute set S. Each element
  /// of S is one attribute path, so S = {["b","c"]} is the single attribute
  /// v.b.c.
  [[nodiscard]] virtual auto distinctValues(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> DistinctEstimate = 0;

  /// @brief can an index serve a probe by these attributes, i.e. a lookup
  /// rather than a scan per outer row? One of them must be its leading field.
  [[nodiscard]] virtual auto hasIndexCovering(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> bool = 0;
};

}  // namespace arangodb::aql
