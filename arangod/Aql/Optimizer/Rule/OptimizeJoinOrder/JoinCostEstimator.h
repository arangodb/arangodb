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

#include <memory>
#include <span>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief the running estimate for one join prefix.
struct JoinEstimate {
  /// @brief n_i : estimated rows produced by the prefix.
  double cardinality = 0.0;
  /// @brief c_i : accumulated cost of the prefix.
  double cost = 0.0;
  /// @brief true when any statistic feeding this estimate was a fallback.
  bool defaulted = false;
};

/// @brief costs the incremental growth of a join prefix.
class JoinCostEstimator {
 public:
  virtual ~JoinCostEstimator() = default;

  /// @brief the estimate for a prefix consisting of `start` alone.
  virtual auto seed(JoinGraph::Node const& start) const -> JoinEstimate = 0;

  /// @brief extend the prefix by `next`, joined via `connecting` *all* edges
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
