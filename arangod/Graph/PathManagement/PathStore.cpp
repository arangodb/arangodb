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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "PathStore.h"
#include "Graph/PathManagement/PathResult.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Types/ValidationResult.h"

#include <Logger/LogMacros.h>
#include <Logger/Logger.h>

using namespace arangodb;

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

template <class Step>
PathStore<Step>::PathStore(arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor) {
  // performance optimization: just reserve a little more as per default
  LOG_TOPIC("47891", TRACE, Logger::GRAPHS) << "<PathStore> Initialization.";
  _schreier.reserve(32);
}

template <class Step>
PathStore<Step>::~PathStore() {
  reset();
}

template <class Step>
void PathStore<Step>::reset() {
  LOG_TOPIC("8f726", TRACE, Logger::GRAPHS) << "<PathStore> Resetting.";
  if (!_schreier.empty()) {
    _resourceMonitor.decreaseMemoryUsage(_schreier.size() * sizeof(Step));
    _schreier.clear();
  }
}

template <class Step>
size_t PathStore<Step>::append(Step step) {
  LOG_TOPIC("45bf4", TRACE, Logger::GRAPHS)
      << "<PathStore> Adding step: " << step.toString();

  auto idx = _schreier.size();
  _resourceMonitor.increaseMemoryUsage(sizeof(Step));
  _schreier.emplace_back(std::move(step));

  return idx;
}

template <class Step>
template <class ProviderType>
auto PathStore<Step>::buildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const
    -> void {
  Step const* myStep = &vertex;

  while (!myStep->isFirst()) {
    path.prependVertex(myStep->getVertex());
    TRI_ASSERT(myStep->getEdge().isValid());
    path.prependEdge(myStep->getEdge());

    TRI_ASSERT(size() > myStep->getPrevious());
    myStep = &_schreier[myStep->getPrevious()];
  }
  path.prependVertex(myStep->getVertex());
}

template <class Step>
template <class ProviderType>
auto PathStore<Step>::reverseBuildPath(Step const& vertex,
                                       PathResult<ProviderType, Step>& path) const
    -> void {
  // For backward we just need to attach ourself
  // So everything until here should be done.
  // We never start with an empty path here, the other side should at least have
  // added the vertex
  TRI_ASSERT(!path.isEmpty());
  if (vertex.isFirst()) {
    // already started at the center.
    // Can stop here
    // The buildPath of the other side has included the vertex already
    return;
  }

  TRI_ASSERT(size() > vertex.getPrevious());
  // We have added the vertex, but we still need the edge on the other side of the path

  TRI_ASSERT(vertex.getEdge().isValid());
  path.appendEdge(vertex.getEdge());

  Step const* myStep = &_schreier[vertex.getPrevious()];

  while (!myStep->isFirst()) {
    path.appendVertex(myStep->getVertex());
    TRI_ASSERT(myStep->getEdge().isValid());
    path.appendEdge(myStep->getEdge());

    TRI_ASSERT(size() > myStep->getPrevious());
    myStep = &_schreier[myStep->getPrevious()];
  }
  path.appendVertex(myStep->getVertex());
}

template <class Step>
auto PathStore<Step>::visitReversePath(Step const& step,
                                       std::function<bool(Step const&)> const& visitor) const
    -> bool {
  Step const* walker = &step;
  // Guaranteed to make progress, as the schreier vector contains a loop-free tree.
  while (true) {
    bool cont = visitor(*walker);
    if (!cont) {
      // Aborted
      return false;
    }
    if (walker->isFirst()) {
      // Visited the full path
      return true;
    }
    walker = &_schreier.at(walker->getPrevious());
  }
}

/* SingleServerProvider Section */

template class PathStore<SingleServerProvider::Step>;
template void PathStore<SingleServerProvider::Step>::buildPath<SingleServerProvider>(
    SingleServerProvider::Step const& vertex,
    PathResult<SingleServerProvider, SingleServerProvider::Step>& path) const;
template void PathStore<SingleServerProvider::Step>::reverseBuildPath<SingleServerProvider>(
    SingleServerProvider::Step const& vertex,
    PathResult<SingleServerProvider, SingleServerProvider::Step>& path) const;

// Tracing
template void PathStore<SingleServerProvider::Step>::buildPath<ProviderTracer<SingleServerProvider>>(
    ProviderTracer<SingleServerProvider>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider>, ProviderTracer<SingleServerProvider>::Step>& path) const;
template void PathStore<SingleServerProvider::Step>::reverseBuildPath<ProviderTracer<SingleServerProvider>>(
    ProviderTracer<SingleServerProvider>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider>, ProviderTracer<SingleServerProvider>::Step>& path) const;

/* ClusterProvider Section */

template class PathStore<ClusterProvider::Step>;
template void PathStore<ClusterProvider::Step>::buildPath<ClusterProvider>(
    ClusterProvider::Step const& vertex,
    PathResult<ClusterProvider, ClusterProvider::Step>& path) const;
template void PathStore<ClusterProvider::Step>::reverseBuildPath<ClusterProvider>(
    ClusterProvider::Step const& vertex,
    PathResult<ClusterProvider, ClusterProvider::Step>& path) const;

// Tracing
template void PathStore<ClusterProvider::Step>::buildPath<ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;
template void PathStore<ClusterProvider::Step>::reverseBuildPath<ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;

}  // namespace graph
}  // namespace arangodb
