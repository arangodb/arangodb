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
#include <cmath>
#include <utility>

namespace arangodb::aql {
namespace {

/// @brief a path repeated across FILTER statements is recorded once per
/// statement; dividing by its distinct count twice would double-count it.
auto dedupe(std::vector<AttributePath> const& paths)
    -> std::vector<AttributePath> {
  std::vector<AttributePath> result = paths;
  std::ranges::sort(result);
  result.erase(std::ranges::unique(result).begin(), result.end());
  return result;
}

/// @brief one index lookup per outer row. The descent walks the whole index,
/// so `collectionSize` is the unrestricted count. Floored at one per row so a
/// one-document collection does not make a join free.
auto probeCost(double rows, double collectionSize) noexcept -> double {
  return rows * std::max(std::log2(collectionSize), 1.0);
}

/// @brief one full scan per outer row, i.e. no usable index.
auto scanCost(double rows, double collectionSize) noexcept -> double {
  return rows * collectionSize;
}

}  // namespace

SystemRCostEstimator::SystemRCostEstimator(
    std::unique_ptr<JoinStatistics> statistics)
    : _statistics(std::move(statistics)) {
  ADB_PROD_ASSERT(_statistics != nullptr);
}

auto SystemRCostEstimator::restrictedFor(JoinGraph::Node const& node) const
    -> Restricted {
  double const count = std::max(_statistics->documentCount(node), 1.0);
  auto const conditions = dedupe(node.conditions);
  auto const distinct = _statistics->distinctValues(node, conditions);

  Restricted value;
  value.defaulted = distinct.defaulted;
  value.rows = std::clamp(count / std::max(distinct.value, 1.0), 1.0, count);
  return value;
}

auto SystemRCostEstimator::seed(JoinGraph::Node const& start) const
    -> JoinEstimate {
  auto const restricted = restrictedFor(start);

  JoinEstimate estimate;
  estimate.cardinality = restricted.rows;
  estimate.defaulted = restricted.defaulted;
  // an index over the constant restrictions turns the scan into a lookup
  estimate.cost = _statistics->hasIndexCovering(start, start.conditions)
                      ? restricted.rows
                      : std::max(_statistics->documentCount(start), 1.0);
  return estimate;
}

auto SystemRCostEstimator::extend(
    JoinEstimate const& prefix, JoinGraph::Node const& next,
    std::span<JoinGraph::Edge const* const> connecting) const -> JoinEstimate {
  auto const nextRestricted = restrictedFor(next);

  JoinEstimate estimate;
  estimate.defaulted = prefix.defaulted || nextRestricted.defaulted;

  double factor = 1.0;
  bool probeable = false;

  for (auto const* edge : connecting) {
    bool const nextIsTo = (edge->to == &next);
    ADB_PROD_ASSERT(nextIsTo || edge->from == &next);

    auto const& nextAttributes =
        nextIsTo ? edge->toAttributes : edge->fromAttributes;
    auto const& otherAttributes =
        nextIsTo ? edge->fromAttributes : edge->toAttributes;
    JoinGraph::Node const* other = nextIsTo ? edge->from : edge->to;

    auto const otherRestricted = restrictedFor(*other);
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
  // a join cannot produce a fraction of a row
  estimate.cardinality =
      std::max(prefix.cardinality * nextRestricted.rows * factor, 1.0);
  estimate.cost =
      prefix.cost + (probeable ? probeCost(prefix.cardinality, count)
                               : scanCost(prefix.cardinality, count));
  return estimate;
}

auto makeDefaultJoinCostEstimator(ExecutionPlan const& plan)
    -> std::unique_ptr<JoinCostEstimator> {
  return std::make_unique<SystemRCostEstimator>(
      std::make_unique<IndexJoinStatistics>(plan));
}

}  // namespace arangodb::aql
