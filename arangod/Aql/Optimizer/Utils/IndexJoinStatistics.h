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

#include "Aql/Optimizer/Utils/JoinStatistics.h"
#include "Basics/AttributeNameParser.h"
#include "Indexes/Index.h"

#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief the index properties this model consults, lifted out of Index so
/// the selection rules below can be exercised without a storage engine (no
/// real collection, no registered indexes -- just scripted facts).
struct IndexFacts {
  Index::IndexType type = Index::TRI_IDX_TYPE_UNKNOWN;
  std::vector<std::vector<basics::AttributeName>> fields;
  bool hidden = false;
  bool inProgress = false;
  bool sparse = false;
  bool hasSelectivityEstimate = false;
  // Only meaningful when hasSelectivityEstimate is true; the Index contract
  // forbids calling Index::selectivityEstimate() otherwise.
  double selectivityEstimate = 0.0;
};

/// @brief |C_S| under the subset rule: consider only candidates whose fields
/// are a subset of `attributes`, and take the maximum of
/// selectivityEstimate() * count over them -- a subset-covering index yields
/// a lower bound on distinct(attributes), which is the conservative
/// direction; a superset would over-estimate distinctness and therefore
/// under-estimate the join. Structural prerequisites (allowed type, not
/// hidden, not in progress, not sparse, no expanded/array field, a usable
/// selectivity estimate in (0, 1]) are enforced the same way regardless of
/// where the facts came from. Falls back to {1.0, defaulted = true} -- the
/// identity for both max() and the estimator's division -- when nothing
/// qualifies, which includes the empty-attribute-set case (no real index
/// has an empty field list, so nothing can ever look like a subset of it;
/// that case is instead the trivial "exactly one empty tuple", handled here
/// directly rather than left to fall out of the loop).
auto distinctFromIndexFacts(std::span<IndexFacts const> candidates,
                            double count,
                            std::span<AttributePath const> attributes)
    -> DistinctEstimate;

/// @brief can any candidate serve a *probe* by these attributes, i.e. an
/// index lookup rather than a full scan per outer row? This is a
/// leading-field question, not a subset one: an index on (y,x) cannot serve
/// a probe by x alone. No selectivity estimate is required, only existence.
auto coveringFromIndexFacts(std::span<IndexFacts const> candidates,
                            std::span<AttributePath const> attributes) -> bool;

/// @brief statistics read from whatever indexes happen to exist on the
/// collections. This runs before index selection, so it consults the
/// collection's indexes directly rather than any IndexNode.
///
/// Depends only on the abstract Index interface: no concrete index class is
/// named and no storage engine is assumed. The selection rules themselves
/// live in distinctFromIndexFacts()/coveringFromIndexFacts() above, over the
/// engine-independent IndexFacts; this class is just the thin adapter that
/// reads real Index objects into that shape.
class IndexJoinStatistics final : public JoinStatistics {
 public:
  explicit IndexJoinStatistics(ExecutionPlan const& plan);

  auto documentCount(JoinGraph::Node const& node) const -> double override;

  auto distinctValues(JoinGraph::Node const& node,
                      std::span<AttributePath const> attributes) const
      -> DistinctEstimate override;

  auto hasIndexCovering(JoinGraph::Node const& node,
                        std::span<AttributePath const> attributes) const
      -> bool override;

 private:
  ExecutionPlan const& _plan;
  // The greedy issues O(n^2) statistics queries and every uncached one walks
  // the collection's index list, so both lookups are memoised. Keyed on the
  // EnumerateCollectionNode -- not the JoinGraph::Node address -- because a
  // JoinGraph is rebuilt per spine run while this object is expected to live
  // for the whole plan, so a freed Node's address could otherwise be reused
  // by an unrelated later graph's node (see SystemRCostEstimator's analogous
  // cache for the same reasoning).
  mutable std::unordered_map<EnumerateCollectionNode const*, double> _counts;
  mutable std::unordered_map<std::string, DistinctEstimate> _distinct;
  mutable std::unordered_map<std::string, bool> _covering;
};

}  // namespace arangodb::aql
