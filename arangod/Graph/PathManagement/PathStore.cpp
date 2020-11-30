////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"

#include <Logger/LogMacros.h>
#include <Logger/Logger.h>

using namespace arangodb;

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

template <class Step>
PathStore<Step>::PathStore() {
  // performance optimization: just reserve a little more as per default
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS) << "<PathStore> Initialization.";
  _schreier.reserve(32);
}

template <class Step>
void PathStore<Step>::reset() {
  LOG_TOPIC("8f726", TRACE, Logger::GRAPHS) << "<PathStore> Resetting.";
  _schreier.clear();
}

template <class Step>
size_t PathStore<Step>::append(Step step) {
  LOG_TOPIC("45bf4", TRACE, Logger::GRAPHS)
      << "<PathStore> Adding step: " << step.toString();

  auto idx = _schreier.size();
  _schreier.emplace_back(step);

  return idx;
}

template <class Step>
template <class ProviderType>
void PathStore<Step>::buildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const {
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
void PathStore<Step>::reverseBuildPath(Step const& vertex,
                                       PathResult<ProviderType, Step>& path) const {
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
bool PathStore<Step>::testPath(Step step) {
  LOG_TOPIC("2ff8d", TRACE, Logger::GRAPHS) << "<PathStore> Testing path:";
  // TODO: needs to be implemented
  return true;
}

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

}  // namespace graph
}  // namespace arangodb
