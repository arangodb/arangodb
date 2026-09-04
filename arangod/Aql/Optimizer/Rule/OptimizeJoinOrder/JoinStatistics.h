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
  /// @brief |C_S| : estimated number of distinct *tuples* over the attribute
  /// set S. For S = {x,y} this is the count of distinct (x,y) combinations,
  /// not distinct(x) * distinct(y) and not either one alone. A count, not a
  /// ratio; `double` because it is an estimate that feeds into products.
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

  /// @brief |C_v| : documents in the node's collection.
  virtual auto documentCount(JoinGraph::Node const& node) const -> double = 0;

  /// @brief |C_S| : distinct tuples over the attribute set S. Note the two
  /// levels of vector mean different things -- AttributePath is itself a
  /// vector, so S = {["b","c"]} is the *single* attribute v.b.c and the tuple
  /// arity is S.size(), not the length of any path within it.
  virtual auto distinctValues(JoinGraph::Node const& node,
                              std::span<AttributePath const> attributes) const
      -> DistinctEstimate = 0;

  /// @brief can an index serve a *probe* by these attributes, i.e. an index
  /// lookup rather than a full scan per outer row? This is a leading-field
  /// question, not a subset one, and needs no selectivity estimate.
  virtual auto hasIndexCovering(JoinGraph::Node const& node,
                                std::span<AttributePath const> attributes) const
      -> bool = 0;
};

}  // namespace arangodb::aql
