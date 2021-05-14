////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "OneSidedEnumerator.h"

#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include "Futures/Future.h"
#include "Graph/Options/OneSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/LifoQueue.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/algorithm-aliases.h"

#include <Graph/Types/ValidationResult.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class Configuration>
OneSidedEnumerator<Configuration>::OneSidedEnumerator(Provider&& forwardProvider,
                                                      OneSidedEnumeratorOptions&& options,
                                                      arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _queue(resourceMonitor),
      _provider(std::move(forwardProvider)),
      _validator(_interior),
      _interior(resourceMonitor) {}

template <class Configuration>
OneSidedEnumerator<Configuration>::~OneSidedEnumerator() {}

template <class Configuration>
auto OneSidedEnumerator<Configuration>::destroyEngines() -> void {
  _provider.destroyEngines();
}

template <class Configuration>
void OneSidedEnumerator<Configuration>::clear() {
  _interior.reset();
  _queue.clear();
  _results.clear();
}

template <class Configuration>
auto OneSidedEnumerator<Configuration>::computeNeighbourhoodOfNextVertex() -> void {
  // Pull next element from Queue
  // Do 1 step search
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);

    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();

    TRI_ASSERT(preparedEnds.size() != 0);
    TRI_ASSERT(_queue.hasProcessableElement());
  }

  auto step = _queue.pop();
  auto posPrevious = _interior.append(step);

  ValidationResult res = _validator.validatePath(step);

  // TODO: Adjust log output
  LOG_TOPIC("78155", TRACE, Logger::GRAPHS)
      << std::boolalpha << "<Traverser> Validated Vertex: " << step.getVertex().getID()
      << " filtered " << res.isFiltered() << " pruned " << res.isPruned()
      << " depth " << _options.getMinDepth() << " <= " << step.getDepth()
      << "<= " << _options.getMaxDepth();
  if (step.getDepth() >= _options.getMinDepth() && !res.isFiltered()) {
    // Include it in results.
    _results.push_back(step);
  }

  if (step.getDepth() < _options.getMaxDepth() && !res.isPruned()) {
    _provider.expand(step, posPrevious, [&](Step n) -> void { _queue.append(n); });
  }
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template <class Configuration>
bool OneSidedEnumerator<Configuration>::isDone() const {
  return _results.empty() && searchDone();
}

/**
 * @brief Reset to new source and target vertices.
 * This API uses string references, this class will not take responsibility
 * for the referenced data. It is caller's responsibility to retain the
 * underlying data and make sure the StringRefs stay valid until next
 * call of reset.
 *
 * @param source The source vertex to start the paths
 */
template <class Configuration>
void OneSidedEnumerator<Configuration>::reset(VertexRef source, size_t depth) {
  clear();
  auto firstStep = _provider.startVertex(source, depth);
  _queue.append(std::move(firstStep));
}

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
template <class Configuration>
auto OneSidedEnumerator<Configuration>::getNextPath()
    -> std::unique_ptr<PathResultInterface> {
  while (!isDone()) {
    searchMoreResults();

    while (!_results.empty()) {
      auto step = std::move(_results.back());
      _results.pop_back();
      return std::make_unique<ResultPathType>(std::move(step), _provider, _interior);
    }
  }
  return nullptr;
}

template <class Configuration>
void OneSidedEnumerator<Configuration>::searchMoreResults() {
  while (_results.empty() && !searchDone()) {  // TODO: check && !_queue.isEmpty()
    _resultsFetched = false;
    computeNeighbourhoodOfNextVertex();
  }

  fetchResults();
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template <class Configuration>
bool OneSidedEnumerator<Configuration>::skipPath() {
  while (!isDone()) {
    searchMoreResults();

    while (!_results.empty()) {
      // just drop one result for skipping
      _results.pop_back();
      return true;
    }
  }
  return false;
}

template <class Configuration>
auto OneSidedEnumerator<Configuration>::searchDone() const -> bool {
  return _queue.isEmpty();
}

template <class Configuration>
auto OneSidedEnumerator<Configuration>::fetchResults() -> void {
  if (!_resultsFetched && !_results.empty()) {
    std::vector<Step*> looseEnds{};

    for (auto& vertex : _results) {
      if (!vertex.isProcessable()) {
        looseEnds.emplace_back(&vertex);
      }
    }

    if (!looseEnds.empty()) {
      // Will throw all network errors here
      futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
      futureEnds.get();
      // Notes for the future:
      // Vertices are now fetched. Think about other less-blocking and
      // batch-wise fetching (e.g. re-fetch at some later point).
      // TODO: Discuss how to optimize here. Currently we'll mark looseEnds in
      // fetch as fetched. This works, but we might create a batch limit here in
      // the future. Also discuss: Do we want (re-)fetch logic here?
      // TODO: maybe we can combine this with prefetching of paths
    }
  }

  _resultsFetched = true;
}

template <class Configuration>
auto OneSidedEnumerator<Configuration>::stealStats() -> aql::TraversalStats {
  aql::TraversalStats stats = _provider.stealStats();
  return stats;
}

/* SingleServerProvider Section */

// Breadth First Search
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::PATH, false>>;
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::PATH, true>>;
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::NONE, false>>;
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::NONE, true>>;
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::GLOBAL, false>>;
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<SingleServerProvider, VertexUniquenessLevel::GLOBAL, true>>;

// Depth First Search
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::PATH, false>>;
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::PATH, true>>;
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::NONE, false>>;
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::NONE, true>>;
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::GLOBAL, false>>;
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<SingleServerProvider, VertexUniquenessLevel::GLOBAL, true>>;

