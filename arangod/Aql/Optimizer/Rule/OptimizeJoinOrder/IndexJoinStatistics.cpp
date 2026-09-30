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

#include "IndexJoinStatistics.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

#include <algorithm>
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

/// @brief structural prerequisites. `sparse` is
/// rejected because a sparse index's estimate is relative to the indexed
/// documents only; expanded (array) fields have different selectivity
/// semantics.
auto isUsable(IndexFacts const& facts) noexcept -> bool {
  return isAllowedType(facts) && !facts.sparse && !hasExpandedField(facts);
}

auto fieldEquals(std::vector<basics::AttributeName> const& field,
                 AttributePath const& path) -> bool {
  if (field.size() != path.size()) {
    return false;
  }
  for (size_t i = 0; i < field.size(); ++i) {
    if (field[i].shouldExpand || field[i].name != path[i]) {
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

}  // namespace

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
  return {std::clamp(best, 1.0, std::max(count, 1.0)), false};
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
    // A probe needs the *leading* field: an index on (y,x) cannot serve a
    // probe by x alone. No selectivity estimate is required, only
    // existence.
    if (facts.fields.empty()) {
      continue;
    }
    auto const& leading = facts.fields.front();
    for (auto const& path : attributes) {
      if (fieldEquals(leading, path)) {
        return true;
      }
    }
  }
  return false;
}

IndexJoinStatistics::IndexJoinStatistics(ExecutionPlan const& plan)
    : _plan(plan) {}

auto IndexJoinStatistics::factsFor(JoinGraph::Node const& node) const
    -> NodeFacts const& {
  auto const id = node.executionNode->id();
  if (auto it = _facts.find(id); it != _facts.end()) {
    return it->second;
  }

  NodeFacts facts;
  for (auto const& index : node.executionNode->collection()->indexes()) {
    if (index != nullptr) {
      facts.indexes.push_back(toIndexFacts(*index));
    }
  }
  auto& trx = _plan.getAst()->query().trxForOptimization();
  if (trx.status() == transaction::Status::RUNNING) {
    facts.count = static_cast<double>(node.executionNode->collection()->count(
        &trx, transaction::CountType::kTryCache));
  }
  return _facts.emplace(id, std::move(facts)).first->second;
}

auto IndexJoinStatistics::documentCount(JoinGraph::Node const& node) const
    -> double {
  return factsFor(node).count;
}

auto IndexJoinStatistics::distinctValues(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> DistinctEstimate {
  if (attributes.empty()) {
    // No restriction at all. This is the empty-subset case of the rule below,
    // not an exception to it. It must not be reported as a guess.
    return {1.0, false};  // Empty set.
  }

  DistinctEstimate estimate{1.0, true};
  auto& trx = _plan.getAst()->query().trxForOptimization();
  if (trx.status() == transaction::Status::RUNNING) {
    auto const& facts = factsFor(node);
    estimate = distinctFromIndexFacts(facts.indexes, facts.count, attributes);
  }

  return estimate;
}

auto IndexJoinStatistics::hasIndexCovering(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> bool {
  if (attributes.empty()) {
    return false;
  }

  bool const covering =
      coveringFromIndexFacts(factsFor(node).indexes, attributes);

  return covering;
}

}  // namespace arangodb::aql
