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

#include "CachingJoinStatistics.h"

#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Assertions/ProdAssert.h"

#include <boost/container_hash/hash.hpp>

#include <functional>
#include <utility>

namespace arangodb::aql {
namespace {

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

CachingJoinStatistics::CachingJoinStatistics(
    std::unique_ptr<JoinStatistics> inner)
    : _inner(std::move(inner)) {
  ADB_PROD_ASSERT(_inner != nullptr);
}

auto CachingJoinStatistics::documentCount(JoinGraph::Node const& node) const
    -> double {
  auto const id = node.executionNode->id();
  if (auto it = _counts.find(id); it != _counts.end()) {
    return it->second;
  }
  return _counts.emplace(id, _inner->documentCount(node)).first->second;
}

auto CachingJoinStatistics::distinctValues(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> DistinctEstimate {
  auto key = keyFor(node, attributes);
  if (auto it = _distinct.find(key); it != _distinct.end()) {
    return it->second;
  }
  auto const estimate = _inner->distinctValues(node, attributes);
  _distinct.emplace(std::move(key), estimate);
  return estimate;
}

auto CachingJoinStatistics::hasIndexCovering(
    JoinGraph::Node const& node,
    std::span<AttributePath const> attributes) const -> bool {
  auto key = keyFor(node, attributes);
  if (auto it = _covering.find(key); it != _covering.end()) {
    return it->second;
  }
  bool const covering = _inner->hasIndexCovering(node, attributes);
  _covering.emplace(std::move(key), covering);
  return covering;
}

}  // namespace arangodb::aql
