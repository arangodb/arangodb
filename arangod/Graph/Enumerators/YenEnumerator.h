////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/HashSetFwd.h"

#include "Basics/ResourceUsage.h"

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/PathManagement/PathStore.h"

#include <vector>

namespace arangodb {

namespace aql {
class TraversalStats;
}

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {

class GraphArena {
  // This is an object which tracks allocations which are needed to store
  // vertex and edge data for references we keep to stay valid.
  static constexpr size_t const kBatchSize = 16384;
  struct Batch {
    std::vector<char> buffer;
    size_t nextFree;
    Batch(size_t capacity = kBatchSize) : buffer(capacity, 0), nextFree(0) {}
  };
  std::vector<Batch> _data;
  size_t _totalSize;
  arangodb::ResourceMonitor& _resourceMonitor;

 public:
  GraphArena(arangodb::ResourceMonitor& resourceMonitor)
      : _totalSize(0), _resourceMonitor(resourceMonitor) {
    // Start with at least one buffer and make `_data` never empty!
    _data.emplace_back();
    _totalSize += kBatchSize;
    _resourceMonitor.increaseMemoryUsage(kBatchSize);
  }

  ~GraphArena() { _resourceMonitor.decreaseMemoryUsage(_totalSize); }

  arangodb::velocypack::HashedStringRef toOwned(
      arangodb::velocypack::HashedStringRef const& item) {
    char const* place = makeLocalCopy(item.data(), item.size());
    return arangodb::velocypack::HashedStringRef(place, item.size());
  }

  void clear() {
    _resourceMonitor.decreaseMemoryUsage(_totalSize);
    _data.clear();
    _data.emplace_back();
    _totalSize = kBatchSize;
    _resourceMonitor.increaseMemoryUsage(kBatchSize);
  }

  EdgeDocumentToken toOwned(EdgeDocumentToken const& item) {
    // We know this: On a coordinator, an EdgeDocumentToken is not owning
    // its allocation, rather it is pointing to a vpack, which others own.
    // On a DBServer or SingleServer, an EdgeDocumentToken is owning its
    // allocation, since it only consists of two uint64_t values. This is
    // why we only act on coordinators here:
    if (!ServerState::instance()->isCoordinator()) {
      return item;  // A copy, but this is cheap!
    }
    uint8_t const* p = item.vpack();
    VPackSlice v{p};
    velocypack::ValueLength s = v.byteSize();
    auto q = (uint8_t const*)(makeLocalCopy((char const*)p, (size_t)s));
    return EdgeDocumentToken(VPackSlice{q});
  }

 private:
  char* makeLocalCopy(char const* p, size_t s) {
    auto* batch = &_data.back();
    if (s > batch->buffer.size() - batch->nextFree) {
      if (s <= kBatchSize) {
        _data.emplace_back();
        _totalSize += kBatchSize;
        _resourceMonitor.increaseMemoryUsage(kBatchSize);
      } else {
        _data.emplace_back(s);
        _totalSize += s;
        _resourceMonitor.increaseMemoryUsage(s);
      }
      batch = &_data.back();
    }
    char* place = &batch->buffer[batch->nextFree];
    batch->nextFree += s;
    memcpy(place, p, s);
    return place;
  }
};

class PathValidatorOptions;
struct TwoSidedEnumeratorOptions;

template<class ProviderType, class Step>
class PathResult;

// This is a relatively straightforward implementation of Yen's algorithm
// as described here: https://en.wikipedia.org/wiki/Yen%27s_algorithm
// It uses single shortest path algorithms by means of a subobject of
// type `TwoSidedEnumerator` (in the unweighted case) and
// `WeightedTwoSidedEnumerator` (in the weighted case).
// The subtlety lies in the template magic going on all around this.
// The `ProviderType` is used to get actual graph data like neighbours
// out of the system. Essentially, there can be a `SingleServerProvider`
// or a `ClusterProvider`, and it can be wrapped with a tracer wrapper or
// not. This provider is just handed on to the `EnumeratorType`, which is
// essentially an instance of the `TwoSidedEnumerator` or the
// `WeightedTwoSidedEnumerator`. However, we have to be able to forbid
// some vertices and edges. This is handled by putting a wrapper type
// around the PathValidator, we use the `PathValidatorTabooWrapper` for
// this. Therefore, the implementation can be rather compact here and
// uses lots of other functionality to do the heavy lifting.
// Note that some effort is necessary for memory management. Since we
// repeatedly call ShortestPath we must constantly reset our subobject,
// which means that we run into the problem that old references to vertex
// and edge IDs used in previous path results become invalid. Therefore,
// we have to copy all these IDs to our own managed memory area.
// This template is instantiated in 8 different ways across the following
// dimensions:
//   SingleServerProvider/ClusterProvider
//   Unweighted/weighted
//   non-tracing/tracing

template<class ProviderType, class EnumeratorType, bool IsWeighted>
class YenEnumerator {
  enum Direction { FORWARD, BACKWARD };

  using VertexRef = ProviderType::Step::VertexType;
  using Edge = ProviderType::Step::EdgeType;

  using VertexSet =
      arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                    std::equal_to<VertexRef>>;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  using GraphOptions = arangodb::graph::TwoSidedEnumeratorOptions;

 public:
  YenEnumerator(ProviderType&& forwardProvider, ProviderType&& backwardProvider,
                TwoSidedEnumeratorOptions&& options,
                PathValidatorOptions validatorOptions,
                arangodb::ResourceMonitor& resourceMonitor);
  YenEnumerator(YenEnumerator const& other) = delete;
  YenEnumerator& operator=(YenEnumerator const& other) = delete;
  YenEnumerator(YenEnumerator&& other) = delete;
  YenEnumerator& operator=(YenEnumerator&& other) = delete;

  ~YenEnumerator();

  auto clear() -> void;

  /**
   * @brief Quick test if the finder can prove there is no more data available.
   *        It can respond with false, even though there is no path left.
   * @return true There will be no further path.
   * @return false There is a chance that there is more data available.
   */
  [[nodiscard]] bool isDone() const;

  /**
   * @brief Reset to new source and target vertices.
   * This API uses string references, this class will not take responsibility
   * for the referenced data. It is caller's responsibility to retain the
   * underlying data and make sure the strings stay valid until next
   * call of reset.
   *
   * @param source The source vertex to start the paths
   * @param target The target vertex to end the paths
   */
  void reset(VertexRef source, VertexRef target, size_t depth = 0);

  /**
   * For a path, we must transfer vertices and edges, so that we own
   * the memory they reference. This method copies the necessary data
   * into our own GraphArena:
   */
  PathResult<ProviderType, typename ProviderType::Step> toOwned(
      PathResult<ProviderType, typename ProviderType::Step> const& path);

  /**
   * @brief Get the next path, if available written into the result build.
   * The given builder will be not be cleared, this function requires a
   * prepared builder to write into.
   *
   * @param result Input and output, this needs to be an open builder,
   * where the path can be placed into.
   * Can be empty, or an openArray, or the value of an object.
   * Guarantee: Every returned path matches the conditions handed in via
   * options. No path is returned twice, it is intended that paths overlap.
   * @return true Found and written a path, result is modified.
   * @return false No path found, result has not been changed.
   */
  bool getNextPath(arangodb::velocypack::Builder& result);

  /**
   * @brief Skip the next Path, like getNextPath, but does not return the path.
   *
   * @return true Found and skipped a path.
   * @return false No path found.
   */

  bool skipPath();
  auto destroyEngines() -> void;

  /**
   * @brief Return statistics generated since
   * the last time this method was called.
   */
  auto stealStats() -> aql::TraversalStats;

 private:
  std::unique_ptr<EnumeratorType> _shortestPathEnumerator;
  // We need to store paths here. Note that the template type `ProviderType`
  // dictates the types for `VertexRef` and `Edge`. `VertexRef` is a
  // reference not owning its own data, so it can become invalid.
  // `Edge` is sometimes a value type and sometimes a reference (depending
  // on the template instantiation). In any case, we must make sure that
  // the vertices and edges we store here will not become invalid. Therefore,
  // we copy the data into a place which we own, before we put anything
  // in here. That is why we keep a GraphArena here.
  GraphArena _arena;
  std::vector<PathResult<ProviderType, typename ProviderType::Step>>
      _shortestPaths;
  // We keep the following vector sorted by the sorting method implemented
  // by PathResult, which makes higher weight paths less than lower weight
  // paths. Thus, the cheapest path is always last and we can easily
  // deduplicate. See below for a static comparator function we use for
  // std::lower_bound:
  std::vector<
      std::unique_ptr<PathResult<ProviderType, typename ProviderType::Step>>>
      _candidatePaths;
  arangodb::ResourceMonitor& _resourceMonitor;
  size_t _totalMemoryUsageHere;
  bool _isDone;  // shortcut to indicate all is done
  VertexRef _source;
  VertexRef _target;
  bool _isInitialized;

  using PathPtr =
      std::unique_ptr<PathResult<ProviderType, typename ProviderType::Step>>;
  static bool pathComparator(PathPtr const& a, PathPtr const& b) {
    return a->compare(*b) == std::strong_ordering::less;
  }
  static bool pathEquals(PathPtr const& a, PathPtr const& b) {
    return a->compare(*b) == std::strong_ordering::equal;
  }
};
}  // namespace graph
}  // namespace arangodb
