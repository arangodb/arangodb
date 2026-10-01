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

#include "Aql/ExecutionNodeId.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"
#include "Basics/AttributeNameParser.h"
#include "Indexes/IndexType.h"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief the result of a distinct-tuple lookup.
struct DistinctEstimate {
  /// @brief |C_S| : distinct *tuples* over the attribute set S, i.e. for
  /// S = {x,y} the distinct (x,y) combinations. A count, not a ratio.
  double value = 1.0;
  /// @brief true when no index could supply an estimate and `value` is the
  /// fallback of 1.
  bool defaulted = true;
};

/// @brief the index properties this model consults, lifted out of Index so
/// the selection rules can be exercised without a storage engine.
struct IndexFacts {
  IndexType type = IndexType::Unknown;
  std::vector<std::vector<basics::AttributeName>> fields;
  bool sparse = false;
  // 0.0 when the index has none; the Index contract forbids calling
  // Index::selectivityEstimate() then.
  double selectivityEstimate = 0.0;
};

/// @brief |C_S| under the subset rule: consider only candidates whose fields
/// are a subset of `attributes`, and take the maximum of
/// selectivityEstimate() * count over them.
auto distinctFromIndexFacts(std::span<IndexFacts const> candidates,
                            double count,
                            std::span<AttributePath const> attributes)
    -> DistinctEstimate;

auto coveringFromIndexFacts(std::span<IndexFacts const> candidates,
                            std::span<AttributePath const> attributes) -> bool;

struct StatsKey {
  ExecutionNodeId node;
  std::vector<AttributePath> paths;

  auto operator==(StatsKey const&) const -> bool = default;
};

struct StatsKeyHash {
  [[nodiscard]] auto operator()(StatsKey const& key) const noexcept
      -> std::size_t;
};

/// @brief statistics about the data a join graph reads. The engine facts --
/// document counts, the collection's indexes and whether one can serve a
/// probe -- live here together with a cache of every answer, since the
/// search asks the same questions O(n^2) times per rule invocation. What an
/// implementation supplies is only how distinct tuples are estimated.
class JoinStatistics {
 public:
  explicit JoinStatistics(ExecutionPlan const& plan);
  virtual ~JoinStatistics() = default;

  /// @brief |C| : documents in the node's collection.
  [[nodiscard]] auto documentCount(JoinGraph::Node const& node) const -> double;

  /// @brief |C_S| : distinct tuples over the attribute set S. Each element
  /// of S is one attribute path, so S = {["b","c"]} is the single attribute
  /// v.b.c.
  [[nodiscard]] auto distinctValues(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> DistinctEstimate;

  /// @brief can an index serve a probe by these attributes, i.e. a lookup
  /// rather than a scan per outer row? One of them must be its leading field.
  [[nodiscard]] auto hasIndexCovering(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> bool;

 protected:
  virtual auto estimateDistinct(JoinGraph::Node const& node,
                                std::span<AttributePath const> attributes) const
      -> DistinctEstimate = 0;

  /// @brief the engine answers; overridable so a test double needs no engine.
  virtual auto countDocuments(JoinGraph::Node const& node) const -> double;
  virtual auto indexCovers(JoinGraph::Node const& node,
                           std::span<AttributePath const> attributes) const
      -> bool;

  /// @brief the collection's indexes lifted into IndexFacts, once per node.
  [[nodiscard]] auto indexFacts(JoinGraph::Node const& node) const
      -> std::span<IndexFacts const>;

 private:
  ExecutionPlan const& _plan;
  mutable std::unordered_map<ExecutionNodeId, std::vector<IndexFacts>> _facts;
  mutable std::unordered_map<ExecutionNodeId, double> _counts;
  mutable std::unordered_map<StatsKey, DistinctEstimate, StatsKeyHash>
      _distinct;
  mutable std::unordered_map<StatsKey, bool, StatsKeyHash> _covering;
};

}  // namespace arangodb::aql
