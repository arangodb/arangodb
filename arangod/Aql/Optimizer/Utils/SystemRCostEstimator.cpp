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

#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Assertions/ProdAssert.h"

#include <algorithm>
#include <array>
#include <utility>

namespace arangodb::aql {
namespace {

/// @brief the same attribute path may be recorded twice, e.g. from
/// `a.x == 'u' AND a.x == 'v'`. Dividing twice by its distinct count would
/// double-count the restriction.
auto dedupe(std::vector<AttributePath> const& paths)
    -> std::vector<AttributePath> {
  std::vector<AttributePath> result = paths;
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

}  // namespace

auto residualSelectivityFactor(AstNode const* residual,
                               JoinStatistics const& stats,
                               JoinGraph::Node const& node) -> double {
  switch (residual->type) {
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return kRangeSelectivityFactor;

    case NODE_TYPE_OPERATOR_BINARY_IN: {
      if (residual->numMembers() != 2) {
        return 1.0;
      }
      auto const* values = residual->getMemberUnchecked(1);
      if (!values->isArray()) {
        return 1.0;
      }
      auto access = extractAttributeAccess(residual->getMemberUnchecked(0));
      if (!access.has_value()) {
        return 1.0;
      }
      std::array<AttributePath, 1> attributes{std::move(access->second)};
      auto distinct = stats.distinctValues(node, attributes);
      if (distinct.defaulted) {
        return 1.0;
      }
      return std::min(1.0, static_cast<double>(values->numMembers()) /
                               std::max(distinct.value, 1.0));
    }

    default:
      // no principled constant for this shape; do not guess
      return 1.0;
  }
}

SystemRCostEstimator::SystemRCostEstimator(
    std::unique_ptr<JoinStatistics> statistics)
    : _statistics(std::move(statistics)) {
  ADB_PROD_ASSERT(_statistics != nullptr);
}

auto SystemRCostEstimator::restrictedFor(JoinGraph::Node const& node) const
    -> Restricted const& {
  if (auto it = _restricted.find(&node); it != _restricted.end()) {
    return it->second;
  }

  double const count = std::max(_statistics->documentCount(node), 1.0);
  auto const conditions = dedupe(node.conditions);
  auto const distinct = _statistics->distinctValues(node, conditions);

  Restricted value;
  value.defaulted = distinct.defaulted;
  value.restricted =
      std::clamp(count / std::max(distinct.value, 1.0), 1.0, count);

  double base = value.restricted;
  for (auto const* residual : node.residuals) {
    base *= residualSelectivityFactor(residual, *_statistics, node);
  }
  value.base = std::clamp(base, 1.0, count);

  return _restricted.emplace(&node, value).first->second;
}

auto SystemRCostEstimator::seed(JoinGraph::Node const& start) const
    -> JoinEstimate {
  auto const& restricted = restrictedFor(start);

  JoinEstimate estimate;
  estimate.cardinality = clampEstimate(restricted.base);
  estimate.defaulted = restricted.defaulted;
  // An index over the constant restrictions turns the initial scan into a
  // lookup returning restricted(v) rows; otherwise the whole collection is
  // read. Residuals never cheapen this -- they filter afterwards.
  estimate.cost =
      _statistics->hasIndexCovering(start, start.conditions)
          ? clampEstimate(restricted.restricted)
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
    if (edge->from == edge->to) {
      // A self-loop is really a single-node filter, not a join predicate; it
      // constrains nothing about this extension.
      continue;
    }
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
    double const dp = std::min(otherDistinct.value, otherRestricted.base);
    double const dn = std::min(nextDistinct.value, nextRestricted.base);
    factor /= std::max({dp, dn, 1.0});

    probeable =
        probeable || _statistics->hasIndexCovering(next, nextAttributes);
  }

  double const count = std::max(_statistics->documentCount(next), 1.0);
  estimate.cardinality =
      clampEstimate(prefix.cardinality * nextRestricted.base * factor);
  estimate.cost = clampEstimate(
      prefix.cost + (probeable ? probeCost(prefix.cardinality, count)
                               : scanCost(prefix.cardinality, count)));
  return estimate;
}

auto makeDefaultJoinCostEstimator(ExecutionPlan const& /*plan*/)
    -> std::unique_ptr<JoinCostEstimator> {
  // Wired up to IndexJoinStatistics in the next task.
  return nullptr;
}

}  // namespace arangodb::aql
