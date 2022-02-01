////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include <Logger/LogMacros.h>
#include <Logger/Logger.h>

using namespace arangodb;

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

template<class Step>
PathStore<Step>::PathStore(arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor) {
  // performance optimization: just reserve a little more as per default
  LOG_TOPIC("47891", TRACE, Logger::GRAPHS) << "<PathStore> Initialization.";
  _schreier.reserve(32);
}

template<class Step>
PathStore<Step>::~PathStore() {
  reset();
}

template<class Step>
void PathStore<Step>::reset() {
  LOG_TOPIC("8f726", TRACE, Logger::GRAPHS) << "<PathStore> Resetting.";
  if (!_schreier.empty()) {
    _resourceMonitor.decreaseMemoryUsage(_schreier.size() * sizeof(Step));
    _schreier.clear();
  }
}

template<class Step>
size_t PathStore<Step>::append(Step step) {
  LOG_TOPIC("45bf4", TRACE, Logger::GRAPHS)
      << "<PathStore> Adding step: " << step.toString();

  auto idx = _schreier.size();

  ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
  _schreier.emplace_back(std::move(step));

  guard.steal();
  return idx;
}

template<class Step>
Step PathStore<Step>::getStep(size_t position) const {
  TRI_ASSERT(position <= size());
  Step step = _schreier.at(position);
  LOG_TOPIC("45bf5", TRACE, Logger::GRAPHS)
      << "<PathStore> Get step: " << step.toString();

  return step;
}

template<class Step>
Step& PathStore<Step>::getStepReference(size_t position) {
  TRI_ASSERT(position <= size());
  auto& step = _schreier.at(position);
  LOG_TOPIC("45bf6", TRACE, Logger::GRAPHS)
      << "<PathStore> Get step: " << step.toString();

  return step;
}

template<class Step>
template<class PathResultType>
auto PathStore<Step>::buildPath(Step const& vertex, PathResultType& path) const
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

template<class Step>
template<class ProviderType>
auto PathStore<Step>::reverseBuildPath(
    Step const& vertex, PathResult<ProviderType, Step>& path) const -> void {
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
  // We have added the vertex, but we still need the edge on the other side of
  // the path

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

template<class Step>
auto PathStore<Step>::visitReversePath(
    Step const& step, std::function<bool(Step const&)> const& visitor) const
    -> bool {
  Step const* walker = &step;
  // Guaranteed to make progress, as the schreier vector contains a loop-free
  // tree.
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

template<class Step>
auto PathStore<Step>::modifyReversePath(
    Step& step, std::function<bool(Step&)> const& visitor) -> bool {
  Step* walker = &step;
  // Guaranteed to make progress, as the schreier vector contains a loop-free
  // tree.
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
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class PathStore<SingleServerProviderStep>;

template void PathStore<SingleServerProviderStep>::buildPath<PathResult<
    SingleServerProvider<SingleServerProviderStep>, SingleServerProviderStep>>(
    SingleServerProviderStep const& vertex,
    PathResult<SingleServerProvider<SingleServerProviderStep>,
               SingleServerProviderStep>& path) const;

template void PathStore<SingleServerProviderStep>::reverseBuildPath<
    SingleServerProvider<SingleServerProviderStep>>(
    SingleServerProviderStep const& vertex,
    PathResult<SingleServerProvider<SingleServerProviderStep>,
               SingleServerProviderStep>& path) const;

// Tracing

template void PathStore<SingleServerProviderStep>::buildPath<PathResult<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>(
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step const&
        vertex,
    PathResult<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>&
        path) const;

template void PathStore<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>::
    reverseBuildPath<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>>(
        ProviderTracer<
            SingleServerProvider<SingleServerProviderStep>>::Step const& vertex,
        PathResult<
            ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
            ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::
                Step>& path) const;

#ifdef USE_ENTERPRISE
template class PathStore<enterprise::SmartGraphStep>;

template void PathStore<enterprise::SmartGraphStep>::buildPath<
    PathResult<SingleServerProvider<enterprise::SmartGraphStep>,
               enterprise::SmartGraphStep>>(
    enterprise::SmartGraphStep const& vertex,
    PathResult<SingleServerProvider<enterprise::SmartGraphStep>,
               enterprise::SmartGraphStep>& path) const;

template void PathStore<enterprise::SmartGraphStep>::reverseBuildPath<
    SingleServerProvider<enterprise::SmartGraphStep>>(
    enterprise::SmartGraphStep const& vertex,
    PathResult<SingleServerProvider<enterprise::SmartGraphStep>,
               enterprise::SmartGraphStep>& path) const;

// Tracing

template void PathStore<enterprise::SmartGraphStep>::buildPath<PathResult<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>>(
    ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step const& vertex,
    PathResult<
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>&
        path) const;

template void PathStore<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>::
    reverseBuildPath<
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>>(
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::
            Step const& vertex,
        PathResult<
            ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
            ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::
                Step>& path) const;
#endif

/* ClusterProvider Section */

template class PathStore<ClusterProvider::Step>;
template void PathStore<ClusterProvider::Step>::buildPath<
    PathResult<ClusterProvider, ClusterProvider::Step>>(
    ClusterProvider::Step const& vertex,
    PathResult<ClusterProvider, ClusterProvider::Step>& path) const;

template void
PathStore<ClusterProvider::Step>::reverseBuildPath<ClusterProvider>(
    ClusterProvider::Step const& vertex,
    PathResult<ClusterProvider, ClusterProvider::Step>& path) const;

// Tracing
template void PathStore<ClusterProvider::Step>::buildPath<PathResult<
    ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>,
               ProviderTracer<ClusterProvider>::Step>& path) const;

template void PathStore<ClusterProvider::Step>::reverseBuildPath<
    ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>,
               ProviderTracer<ClusterProvider>::Step>& path) const;

}  // namespace graph
}  // namespace arangodb
