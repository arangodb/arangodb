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

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief ceiling for cardinalities and costs. A long chain of defaulted
/// statistics would otherwise reach infinity, and inf - inf is nan, which
/// would break the ordering comparison. A large finite number keeps the
/// comparison total.
constexpr double kMaxEstimate = 1e18;

inline auto clampEstimate(double value) noexcept -> double {
  if (!std::isfinite(value) || value > kMaxEstimate) {
    return kMaxEstimate;
  }
  return std::max(value, 0.0);
}

/// @brief cost of one index lookup per outer row. The descent traverses the
/// whole index, so `collectionSize` is the *unrestricted* document count: a
/// constant restriction on the inner side is an extra predicate and does not
/// shrink the index. Floored at 1.0 per row so a one-document collection does
/// not make a join free.
///
/// This and scanCost encode the execution engine, not the data distribution,
/// so an alternative cardinality model reuses them unchanged.
inline auto probeCost(double rows, double collectionSize) noexcept -> double {
  double const perRow = std::max(std::log2(std::max(collectionSize, 1.0)), 1.0);
  return clampEstimate(rows * perRow);
}

/// @brief cost of one full scan per outer row, i.e. no usable index.
inline auto scanCost(double rows, double collectionSize) noexcept -> double {
  return clampEstimate(rows * std::max(collectionSize, 1.0));
}

/// @brief the running estimate for one join prefix.
struct JoinEstimate {
  /// @brief n_i : estimated rows produced by the prefix.
  double cardinality = 0.0;
  /// @brief c_i : accumulated cost of the prefix.
  double cost = 0.0;
  /// @brief true when any statistic feeding this estimate was a fallback.
  bool defaulted = false;
};

/// @brief costs the incremental growth of a join prefix. This is the seam the
/// ordering algorithm codes against; a frequency-based cardinality model would
/// be a second implementation, reusing probeCost/scanCost unchanged.
class JoinCostEstimator {
 public:
  virtual ~JoinCostEstimator() = default;

  /// @brief the estimate for a prefix consisting of `start` alone.
  virtual auto seed(JoinGraph::Node const& start) const -> JoinEstimate = 0;

  /// @brief extend the prefix by `next`, joined via `connecting` -- *all* edges
  /// between `next` and the prefix, because a cycle constrains the new vertex
  /// with more than one predicate. An empty span means a cross product.
  virtual auto extend(JoinEstimate const& prefix, JoinGraph::Node const& next,
                      std::span<JoinGraph::Edge const* const> connecting) const
      -> JoinEstimate = 0;
};

/// @brief the estimator used in production: System-R cardinality over
/// index-backed statistics.
auto makeDefaultJoinCostEstimator(ExecutionPlan const& plan)
    -> std::unique_ptr<JoinCostEstimator>;

}  // namespace arangodb::aql
