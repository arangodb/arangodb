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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinCostEstimator.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"

#include <memory>
#include <span>
#include <unordered_map>

namespace arangodb::aql {

/// @brief the selectivity factors used here are System-R's, from Table 1 of
/// P. Griffiths Selinger, M. M. Astrahan, D. D. Chamberlin, R. A. Lorie and
/// T. G. Price, "Access Path Selection in a Relational Database Management
/// System", SIGMOD 1979, pp. 23-34.

/// @brief Table 1, `column > value`: the factor when the bound is not known at
/// optimization time. The paper is explicit that the number is a guess -- "no
/// significance to this number, other than that it is less selective than the
/// guesses for equal predicates for which there are no indexes, and that it is
/// less than 1/2" -- so it is not a measurement to be tuned.
constexpr double kRangeSelectivityFactor = 1.0 / 3.0;

/// @brief the fraction of `node`'s rows that survive the residual predicate
/// `residual`, in the surviving-fraction sense (1.0 filters nothing -- the
/// opposite direction from Index::selectivityEstimate). Returns 1.0 for any
/// predicate shape without a principled constant, so an unmodelled predicate
/// leaves the estimate untouched rather than nudging it arbitrarily.
auto residualSelectivityFactor(AstNode const* residual,
                               JoinStatistics const& stats,
                               JoinGraph::Node const& node) -> double;

/// @brief System-R cardinality estimation over a pluggable statistics source.
/// |a join b| = |a||b| / max(|a_x|, |b_y|), which is symmetric, paired with the
/// engine-level probe/scan cost recurrence from JoinCostEstimator.h.
class SystemRCostEstimator final : public JoinCostEstimator {
 public:
  explicit SystemRCostEstimator(std::unique_ptr<JoinStatistics> statistics);

  auto seed(JoinGraph::Node const& start) const -> JoinEstimate override;

  auto extend(JoinEstimate const& prefix, JoinGraph::Node const& next,
              std::span<JoinGraph::Edge const* const> connecting) const
      -> JoinEstimate override;

 private:
  /// @brief per-node row counts. `restricted` is after the equality
  /// restrictions only and drives *costs*, because an index lookup returns
  /// that many rows and the residuals filter them afterwards. `base` is after
  /// the residuals too and drives *cardinalities*.
  struct Restricted {
    double restricted = 1.0;
    double base = 1.0;
    bool defaulted = false;
  };

  auto restrictedFor(JoinGraph::Node const& node) const -> Restricted const&;

  std::unique_ptr<JoinStatistics> _statistics;
  // Keyed on the node's out-variable, not the `JoinGraph::Node*` address: the
  // rule builds and destroys one JoinGraph per spine run while this estimator
  // is constructed once per plan, so a freed node's address can be reused by
  // an unrelated later graph's node. Variables are owned by the Ast and stay
  // alive and distinct for the whole plan, so they are a safe cache key.
  mutable std::unordered_map<Variable const*, Restricted> _restricted;
};

}  // namespace arangodb::aql
