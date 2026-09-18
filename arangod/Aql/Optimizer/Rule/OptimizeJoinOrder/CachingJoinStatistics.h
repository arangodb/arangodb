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
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"
#include "Containers/FlatHashMap.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace arangodb::aql {

/// @brief a lookup key that borrows its paths, so a cache probe allocates
/// nothing. The id is a small value type and is simply copied.
struct StatsKeyView {
  ExecutionNodeId node;
  std::span<AttributePath const> paths;
};

struct StatsKey {
  ExecutionNodeId node;
  std::vector<AttributePath> paths;

  [[nodiscard]] auto view() const noexcept -> StatsKeyView {
    return {node, {paths.data(), paths.size()}};
  }
};

/// @brief `is_transparent` is what lets FlatHashMap probe with a StatsKeyView.
/// Without it the map would silently build a StatsKey per lookup, which is the
/// allocation this key exists to remove.
struct StatsKeyHash {
  using is_transparent = void;

  [[nodiscard]] auto operator()(StatsKeyView const& key) const noexcept
      -> std::size_t;
  [[nodiscard]] auto operator()(StatsKey const& key) const noexcept
      -> std::size_t {
    return (*this)(key.view());
  }
};

struct StatsKeyEq {
  using is_transparent = void;

  [[nodiscard]] auto operator()(StatsKeyView const& lhs,
                                StatsKeyView const& rhs) const noexcept -> bool;
  [[nodiscard]] auto operator()(StatsKey const& lhs,
                                StatsKeyView const& rhs) const noexcept
      -> bool {
    return (*this)(lhs.view(), rhs);
  }
  [[nodiscard]] auto operator()(StatsKeyView const& lhs,
                                StatsKey const& rhs) const noexcept -> bool {
    return (*this)(lhs, rhs.view());
  }
  [[nodiscard]] auto operator()(StatsKey const& lhs,
                                StatsKey const& rhs) const noexcept -> bool {
    return (*this)(lhs.view(), rhs.view());
  }
};

/// @brief memoises another JoinStatistics. The search queries the same node
/// and attribute set O(n^2) times while the answers cannot change within one
/// rule invocation, so every implementation faces the same repeated calls.k
class CachingJoinStatistics final : public JoinStatistics {
 public:
  explicit CachingJoinStatistics(std::unique_ptr<JoinStatistics> inner);

  [[nodiscard]] auto documentCount(JoinGraph::Node const& node) const
      -> double override;

  [[nodiscard]] auto distinctValues(JoinGraph::Node const& node,
                                    std::span<AttributePath const> attributes)
      const -> DistinctEstimate override;

  [[nodiscard]] auto hasIndexCovering(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> bool override;

 private:
  std::unique_ptr<JoinStatistics> _inner;
  // Flat rather than node-based: all three accessors return by value,
  // so nothing holds a reference that growth could invalidate when the table
  // moves its elements. These are probed O(n^2) times per rule invocation,
  // which is where storing elements inline pays.
  mutable containers::FlatHashMap<ExecutionNodeId, double> _counts;
  mutable containers::FlatHashMap<StatsKey, DistinctEstimate, StatsKeyHash,
                                  StatsKeyEq>
      _distinct;
  mutable containers::FlatHashMap<StatsKey, bool, StatsKeyHash, StatsKeyEq>
      _covering;
};

}  // namespace arangodb::aql
