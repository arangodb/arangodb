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
#include "JoinStatistics.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

#include <boost/container_hash/hash.hpp>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace arangodb::aql {
namespace {

/// @brief only these index types carry a distinct-value estimate with the
/// semantics this model needs. Inverted, geo, ttl, mdi and vector indexes
/// report unrelated numbers.
auto isAllowedType(IndexFacts const& facts) noexcept -> bool {
  return facts.type == IndexType::Primary || facts.type == IndexType::Edge ||
         facts.type == IndexType::Persistent;
}

auto hasExpandedField(IndexFacts const& facts) noexcept -> bool {
  for (auto const& field : facts.fields) {
    for (auto const& component : field) {
      if (component.shouldExpand) {
        return true;
      }
    }
  }
  return false;
}

/// @brief a sparse index's estimate is relative to the indexed documents
/// only, and an expanded (array) field has different selectivity semantics.
auto isUsable(IndexFacts const& facts) noexcept -> bool {
  return isAllowedType(facts) && !facts.sparse && !hasExpandedField(facts);
}

auto fieldEquals(std::vector<basics::AttributeName> const& field,
                 AttributePath const& path) -> bool {
  if (field.size() != path.size()) {
    return false;
  }
  for (size_t i = 0; i < field.size(); ++i) {
    if (field[i].name != path[i]) {
      return false;
    }
  }
  return true;
}

/// @brief are all of the candidate's fields present in `attributes`? A
/// subset-covering index yields a lower bound on distinct(attributes).
auto fieldsAreSubsetOf(IndexFacts const& facts,
                       std::span<AttributePath const> attributes) -> bool {
  for (auto const& field : facts.fields) {
    bool found = false;
    for (auto const& path : attributes) {
      if (fieldEquals(field, path)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

/// @brief lift the properties this model needs out of a real Index.
auto toIndexFacts(Index const& index) -> IndexFacts {
  IndexFacts facts;
  facts.type = index.type();
  facts.fields = index.fields();
  facts.sparse = index.sparse();
  facts.selectivityEstimate =
      index.hasSelectivityEstimate() ? index.selectivityEstimate() : 0.0;
  return facts;
}

auto keyFor(JoinGraph::Node const& node,
            std::span<AttributePath const> attributes) -> StatsKey {
  return {node.executionNode->id(), {attributes.begin(), attributes.end()}};
}

}  // namespace

auto StatsKeyHash::operator()(StatsKey const& key) const noexcept
    -> std::size_t {
  std::size_t seed = 0;
  boost::hash_combine(seed, std::hash<ExecutionNodeId>{}(key.node));
  boost::hash_range(seed, key.paths.begin(), key.paths.end());
  return seed;
}

auto distinctFromIndexFacts(std::span<IndexFacts const> candidates,
                            double count,
                            std::span<AttributePath const> attributes)
    -> DistinctEstimate {
  if (attributes.empty()) {
    // No restriction at all. The empty-subset case of the rule below, not
    // an exception to it, and it must not be reported as a guess.
    return {1.0, false};
  }

  double best = 1.0;
  bool found = false;

  for (auto const& facts : candidates) {
    if (!isUsable(facts)) {
      continue;
    }
    if (!fieldsAreSubsetOf(facts, attributes)) {
      continue;
    }
    // 0.0 is "no estimate", and an out-of-range value divides by zero
    // downstream.
    double const selectivity = facts.selectivityEstimate;
    if (!(selectivity > 0.0) || selectivity > 1.0) {
      continue;
    }
    best = std::max(best, selectivity * count);
    found = true;
  }

  if (!found) {
    return {1.0, true};
  }
  return {best, false};
}

auto coveringFromIndexFacts(std::span<IndexFacts const> candidates,
                            std::span<AttributePath const> attributes) -> bool {
  if (attributes.empty()) {
    return false;
  }

  for (auto const& facts : candidates) {
    if (!isUsable(facts)) {
      continue;
    }
    // a probe needs the leading field: an index on (y,x) cannot serve x alone
    auto const& leading = facts.fields.front();
    for (auto const& path : attributes) {
      if (fieldEquals(leading, path)) {
        return true;
      }
    }
  }
  return false;
}

JoinStatistics::JoinStatistics(ExecutionPlan const& plan) : _plan(plan) {}

auto JoinStatistics::documentCount(JoinGraph::Node const& node) const
    -> double {
  auto const id = node.executionNode->id();
  if (auto it = _counts.find(id); it != _counts.end()) {
    return it->second;
  }
  return _counts.emplace(id, countDocuments(node)).first->second;
}

auto JoinStatistics::distinctValues(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> DistinctEstimate {
  auto key = keyFor(node, attributes);
  if (auto it = _distinct.find(key); it != _distinct.end()) {
    return it->second;
  }
  auto const estimate = estimateDistinct(node, attributes);
  _distinct.emplace(std::move(key), estimate);
  return estimate;
}

auto JoinStatistics::hasIndexCovering(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> bool {
  auto key = keyFor(node, attributes);
  if (auto it = _covering.find(key); it != _covering.end()) {
    return it->second;
  }
  bool const covering = indexCovers(node, attributes);
  _covering.emplace(std::move(key), covering);
  return covering;
}

auto JoinStatistics::countDocuments(JoinGraph::Node const& node) const
    -> double {
  auto& trx = _plan.getAst()->query().trxForOptimization();
  return static_cast<double>(node.executionNode->collection()->count(
      &trx, transaction::CountType::kTryCache));
}

auto JoinStatistics::indexCovers(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> bool {
  return coveringFromIndexFacts(indexFacts(node), attributes);
}

auto JoinStatistics::indexFacts(JoinGraph::Node const& node) const
    -> std::span<IndexFacts const> {
  auto const id = node.executionNode->id();
  if (auto it = _facts.find(id); it != _facts.end()) {
    return it->second;
  }
  std::vector<IndexFacts> facts;
  for (auto const& index : node.executionNode->collection()->indexes()) {
    facts.push_back(toIndexFacts(*index));
  }
  return _facts.emplace(id, std::move(facts)).first->second;
}

}  // namespace arangodb::aql
