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

#include "SystemRCostEstimator.h"

#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/IndexJoinStatistics.h"
#include "Assertions/ProdAssert.h"

#include <algorithm>
#include <utility>

namespace arangodb::aql {
namespace {

/// @brief the same attribute path may be recorded twice, e.g. when a
/// restriction is repeated across statements: `FILTER a.x == 'u'
/// FILTER a.x == 'u'`. Dividing twice by its distinct count would
/// double-count the restriction.
auto dedupe(std::vector<AttributePath> const& paths)
    -> std::vector<AttributePath> {
  std::vector<AttributePath> result = paths;
  std::ranges::sort(result);
  result.erase(std::ranges::unique(result).begin(), result.end());
  return result;
}

}  // namespace

SystemRCostEstimator::SystemRCostEstimator(
    std::unique_ptr<JoinStatistics> statistics)
    : _statistics(std::move(statistics)) {
  ADB_PROD_ASSERT(_statistics != nullptr);
}

auto SystemRCostEstimator::restrictedFor(JoinGraph::Node const& node) const
    -> Restricted const& {
  auto const* variable = node.executionNode->outVariable();
  if (auto it = _restricted.find(variable); it != _restricted.end()) {
    return it->second;
  }

  double const count = std::max(_statistics->documentCount(node), 1.0);
  auto const conditions = dedupe(node.conditions);
  auto const distinct = _statistics->distinctValues(node, conditions);

  Restricted value;
  value.defaulted = distinct.defaulted;
  value.rows = std::clamp(count / std::max(distinct.value, 1.0), 1.0, count);

  return _restricted.emplace(variable, value).first->second;
}

auto SystemRCostEstimator::seed(JoinGraph::Node const& start) const
    -> JoinEstimate {
  auto const& restricted = restrictedFor(start);

  JoinEstimate estimate;
  estimate.cardinality = clampEstimate(restricted.rows);
  estimate.defaulted = restricted.defaulted;
  // An index over the constant restrictions turns the initial scan into a
  // lookup returning restricted(v) rows; otherwise the whole collection is
  // read.
  estimate.cost =
      _statistics->hasIndexCovering(start, start.conditions)
          ? clampEstimate(restricted.rows)
          : clampEstimate(std::max(_statistics->documentCount(start), 1.0));
  return estimate;
}

auto SystemRCostEstimator::extend(
    JoinEstimate const& prefix, JoinGraph::Node const& next,
    std::span<JoinGraph::Edge const* const> connecting) const -> JoinEstimate {
  auto const& nextRestricted = restrictedFor(next);

  JoinEstimate estimate;
  estimate.defaulted = prefix.defaulted || nextRestricted.defaulted;

  double factor = 1.0;
  bool probeable = false;

  for (auto const* edge : connecting) {
    TRI_ASSERT(edge->from != edge->to);
    bool const nextIsTo = (edge->to == &next);
    ADB_PROD_ASSERT(nextIsTo || edge->from == &next);

    auto const& nextAttributes =
        nextIsTo ? edge->toAttributes : edge->fromAttributes;
    auto const& otherAttributes =
        nextIsTo ? edge->fromAttributes : edge->toAttributes;
    JoinGraph::Node const* other = nextIsTo ? edge->from : edge->to;

    auto const& otherRestricted = restrictedFor(*other);
    auto const otherDistinct =
        _statistics->distinctValues(*other, otherAttributes);
    auto const nextDistinct = _statistics->distinctValues(next, nextAttributes);
    estimate.defaulted =
        estimate.defaulted || otherDistinct.defaulted || nextDistinct.defaulted;

    // A node cannot hold more distinct values than surviving rows.
    double const dp = std::min(otherDistinct.value, otherRestricted.rows);
    double const dn = std::min(nextDistinct.value, nextRestricted.rows);
    factor /= std::max({dp, dn, 1.0});

    probeable =
        probeable || _statistics->hasIndexCovering(next, nextAttributes);
  }

  double const count = std::max(_statistics->documentCount(next), 1.0);
  // Floored at 1.0: a join cannot meaningfully produce less than one row when
  // its output is itself a multiplier feeding later extend() calls. Without
  // this floor, two connecting edges plus an over-restricted next can
  // drive the product arbitrarily far below 1 (clampEstimate only floors at
  // 0), which then charges near-zero cost for every subsequent step and
  // erases the cost differences the search relies on to discriminate between
  // orderings.
  estimate.cardinality = std::max(
      clampEstimate(prefix.cardinality * nextRestricted.rows * factor), 1.0);
  estimate.cost = clampEstimate(
      prefix.cost + (probeable ? probeCost(prefix.cardinality, count)
                               : scanCost(prefix.cardinality, count)));
  return estimate;
}

auto makeDefaultJoinCostEstimator(ExecutionPlan const& plan)
    -> std::unique_ptr<JoinCostEstimator> {
  return std::make_unique<SystemRCostEstimator>(
      std::make_unique<IndexJoinStatistics>(plan));
}

}  // namespace arangodb::aql
