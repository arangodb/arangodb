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
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"

#include <memory>
#include <span>
#include <unordered_map>

namespace arangodb::aql {

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
  /// @brief the node's row count after its constant equality restrictions.
  struct Restricted {
    double rows = 1.0;
    bool defaulted = false;
  };

  auto restrictedFor(JoinGraph::Node const& node) const -> Restricted const&;

  std::unique_ptr<JoinStatistics> _statistics;
  // Keyed on the node's out-variable, not the `JoinGraph::Node*` address: the
  // rule builds and destroys one JoinGraph per spine run while this estimator
  // is constructed once per plan, so a freed node's address can be reused by
  // an unrelated later graph's node. Variables are owned by the Ast and stay
  // alive and distinct for the whole plan, so they are a safe cache key.
  //
  // Node-based deliberately. restrictedFor() hands out a reference into this
  // map and extend() holds one across further calls that insert into it, so a
  // flat map -- which moves its elements on rehash -- would leave that
  // reference dangling. NodeHashMap, not FlatHashMap, if this is modernised.
  mutable std::unordered_map<Variable const*, Restricted> _restricted;
};

}  // namespace arangodb::aql
