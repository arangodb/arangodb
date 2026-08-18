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
#include "Aql/Variable.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

#include <algorithm>
#include <vector>

namespace arangodb::aql {
namespace {

/// @brief only these index types carry a distinct-value estimate with the
/// semantics this model needs. Inverted, geo, ttl, mdi and vector indexes
/// report unrelated numbers.
auto isAllowedType(Index const& index) noexcept -> bool {
  auto const type = index.type();
  return type == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
         type == Index::TRI_IDX_TYPE_EDGE_INDEX ||
         type == Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
}

auto hasExpandedField(Index const& index) noexcept -> bool {
  for (auto const& field : index.fields()) {
    for (auto const& component : field) {
      if (component.shouldExpand) {
        return true;
      }
    }
  }
  return false;
}

/// @brief structural prerequisites shared by both queries. `sparse` is
/// rejected because a sparse index's estimate is relative to the indexed
/// documents only; expanded (array) fields have different selectivity
/// semantics.
auto isUsable(Index const& index) noexcept -> bool {
  return isAllowedType(index) && !index.isHidden() && !index.inProgress() &&
         !index.sparse() && !hasExpandedField(index);
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

/// @brief are all of the index's fields present in `attributes`? A
/// subset-covering index yields a lower bound on distinct(attributes), which
/// is the conservative direction; a superset would over-estimate
/// distinctness.
auto fieldsAreSubsetOf(Index const& index,
                       std::span<AttributePath const> attributes) -> bool {
  for (auto const& field : index.fields()) {
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

auto cacheKey(JoinGraph::Node const& node,
              std::span<AttributePath const> attributes) -> std::string {
  std::vector<std::string> parts;
  parts.reserve(attributes.size());
  for (auto const& path : attributes) {
    std::string joined;
    for (auto const& component : path) {
      if (!joined.empty()) {
        joined += '.';
      }
      joined += component;
    }
    parts.emplace_back(std::move(joined));
  }
  std::sort(parts.begin(), parts.end());

  std::string key = std::to_string(node.executionNode->id().id());
  for (auto const& part : parts) {
    key += '\x1f';
    key += part;
  }
  return key;
}

}  // namespace

IndexJoinStatistics::IndexJoinStatistics(ExecutionPlan const& plan)
    : _plan(plan) {}

auto IndexJoinStatistics::documentCount(JoinGraph::Node const& node) const
    -> double {
  if (auto it = _counts.find(node.executionNode); it != _counts.end()) {
    return it->second;
  }

  double count = 0.0;
  auto& trx = _plan.getAst()->query().trxForOptimization();
  if (trx.status() == transaction::Status::RUNNING) {
    count = static_cast<double>(node.executionNode->collection()->count(
        &trx, transaction::CountType::kTryCache));
  }
  _counts.emplace(node.executionNode, count);
  return count;
}

auto IndexJoinStatistics::distinctValues(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> DistinctEstimate {
  if (attributes.empty()) {
    // No restriction at all. This is the empty-subset case of the rule below,
    // not an exception to it -- and it must not be reported as a guess.
    return {1.0, false};
  }

  auto const key = cacheKey(node, attributes);
  if (auto it = _distinct.find(key); it != _distinct.end()) {
    return it->second;
  }

  // Defaults to "no confident estimate" and stays there unless a qualifying
  // index is actually found while the transaction is running. In particular,
  // if the transaction is not RUNNING, documentCount() would silently read
  // back as 0 (it has no defaulted channel of its own) and a naive
  // selectivity * 0 computation would look like a confident distinct count
  // of 1 for any qualifying index -- an unusable transaction must not
  // masquerade as a confident estimate, so that case is rejected up front
  // rather than being allowed to fall out of the arithmetic.
  DistinctEstimate estimate{1.0, true};
  auto& trx = _plan.getAst()->query().trxForOptimization();
  if (trx.status() == transaction::Status::RUNNING) {
    double const count = documentCount(node);
    double best = 1.0;
    bool found = false;

    for (auto const& index : node.executionNode->collection()->indexes()) {
      if (index == nullptr || !isUsable(*index)) {
        continue;
      }
      if (!index->hasSelectivityEstimate()) {
        continue;
      }
      if (!fieldsAreSubsetOf(*index, attributes)) {
        continue;
      }
      // The Index contract does not promise that hasSelectivityEstimate()
      // yields a usable number, and an out-of-range value divides by zero
      // downstream.
      double const selectivity = index->selectivityEstimate();
      if (!(selectivity > 0.0) || selectivity > 1.0) {
        continue;
      }
      best = std::max(best, selectivity * count);
      found = true;
    }

    if (found) {
      estimate = {std::clamp(best, 1.0, std::max(count, 1.0)), false};
    }
  }

  _distinct.emplace(key, estimate);
  return estimate;
}

auto IndexJoinStatistics::hasIndexCovering(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> bool {
  if (attributes.empty()) {
    return false;
  }

  auto const key = cacheKey(node, attributes);
  if (auto it = _covering.find(key); it != _covering.end()) {
    return it->second;
  }

  bool covering = false;
  for (auto const& index : node.executionNode->collection()->indexes()) {
    if (index == nullptr || !isUsable(*index)) {
      continue;
    }
    // A probe needs the *leading* field: an index on (y,x) cannot serve a
    // probe by x alone. No selectivity estimate is required, only existence.
    auto const& fields = index->fields();
    if (fields.empty()) {
      continue;
    }
    for (auto const& path : attributes) {
      if (fieldEquals(fields.front(), path)) {
        covering = true;
        break;
      }
    }
    if (covering) {
      break;
    }
  }

  _covering.emplace(key, covering);
  return covering;
}

}  // namespace arangodb::aql
