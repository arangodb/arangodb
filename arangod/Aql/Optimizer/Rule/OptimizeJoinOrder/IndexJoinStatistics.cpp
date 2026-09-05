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
  return facts.type == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
         facts.type == Index::TRI_IDX_TYPE_EDGE_INDEX ||
         facts.type == Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
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

/// @brief structural prerequisites shared by both rules. `sparse` is
/// rejected because a sparse index's estimate is relative to the indexed
/// documents only; expanded (array) fields have different selectivity
/// semantics.
auto isUsable(IndexFacts const& facts) noexcept -> bool {
  return isAllowedType(facts) && !facts.hidden && !facts.inProgress &&
         !facts.sparse && !hasExpandedField(facts);
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
/// subset-covering index yields a lower bound on distinct(attributes), which
/// is the conservative direction; a superset would over-estimate
/// distinctness.
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

auto cacheKey(JoinGraph::Node const& node,
              std::span<AttributePath const> attributes) -> std::string {
  std::vector<std::string> parts;
  parts.reserve(attributes.size());
  for (auto const& path : attributes) {
    std::string joined;
    for (auto const& component : path) {
      if (!joined.empty()) {
        // Not '.': it can appear inside an attribute name, so ["a","b"] and
        // ["a.b"] would collide in the cache. '\x1e' joins path components,
        // '\x1f' joins whole paths below.
        joined += '\x1e';
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

/// @brief lift the properties this model needs out of a real Index. The
auto toIndexFacts(Index const& index) -> IndexFacts {
  IndexFacts facts;
  facts.type = index.type();
  facts.fields = index.fields();
  facts.hidden = index.isHidden();
  facts.inProgress = index.inProgress();
  facts.sparse = index.sparse();
  facts.hasSelectivityEstimate = index.hasSelectivityEstimate();
  // The Index contract does not permit calling selectivityEstimate() when
  // hasSelectivityEstimate() is false.
  facts.selectivityEstimate =
      facts.hasSelectivityEstimate ? index.selectivityEstimate() : 0.0;
  return facts;
}

auto candidatesFor(JoinGraph::Node const& node) -> std::vector<IndexFacts> {
  std::vector<IndexFacts> candidates;
  for (auto const& index : node.executionNode->collection()->indexes()) {
    if (index != nullptr) {
      candidates.push_back(toIndexFacts(*index));
    }
  }
  return candidates;
}

}  // namespace

auto distinctFromIndexFacts(std::span<IndexFacts const> candidates,
                            double count,
                            std::span<AttributePath const> attributes)
    -> DistinctEstimate {
  if (attributes.empty()) {
    // No restriction at all -- the empty-subset case of the rule below, not
    // an exception to it, and it must not be reported as a guess.
    return {1.0, false};
  }

  double best = 1.0;
  bool found = false;

  for (auto const& facts : candidates) {
    if (!isUsable(facts)) {
      continue;
    }
    if (!facts.hasSelectivityEstimate) {
      continue;
    }
    if (!fieldsAreSubsetOf(facts, attributes)) {
      continue;
    }
    // The Index contract does not promise that hasSelectivityEstimate()
    // yields a usable number, and an out-of-range value divides by zero
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

/// @brief unlike distinctValues(), this has no `defaulted` channel: an
/// out-of-transaction call silently reads back as 0.0 rather than reporting
/// that it guessed. That is safe only by the combination of two properties,
/// both worth re-checking before trusting this again:
///   1. restrictedFor() clamps this to count = std::max(documentCount, 1.0),
///      so a 0.0 here becomes count = 1 everywhere downstream, which makes
///      probeCost == scanCost for every candidate order -- no order looks
///      cheaper than any other, so this alone cannot steer the greedy.
///   2. any real equijoin also calls distinctValues() with a non-empty
///      attribute set, which *does* honestly report `defaulted = true` when
///      the transaction is not RUNNING (see below) -- so the existing
///      `defaulted` flag still reaches a caller through that second lookup,
///      even though this one is silent.
/// If either property stops holding -- e.g. a caller starts using
/// documentCount for something other than a uniform "all counts are 1"
/// clamp, or a code path reaches this with conditions/residuals empty and no
/// accompanying distinctValues() call -- this reasoning breaks and
/// documentCount needs its own `defaulted` channel.
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

  // Defaults to "no confident estimate" and stays there unless the
  // transaction is actually RUNNING. documentCount() has no defaulted
  // channel of its own (it would silently read back as 0), and feeding that
  // into the rule below for a qualifying index would look like a confident
  // distinct count of 1 -- an unusable transaction must not masquerade as a
  // confident estimate, so that case is rejected up front rather than being
  // allowed to fall out of the arithmetic.
  DistinctEstimate estimate{1.0, true};
  auto& trx = _plan.getAst()->query().trxForOptimization();
  if (trx.status() == transaction::Status::RUNNING) {
    double const count = documentCount(node);
    estimate = distinctFromIndexFacts(candidatesFor(node), count, attributes);
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

  bool const covering = coveringFromIndexFacts(candidatesFor(node), attributes);

  _covering.emplace(key, covering);
  return covering;
}

}  // namespace arangodb::aql
