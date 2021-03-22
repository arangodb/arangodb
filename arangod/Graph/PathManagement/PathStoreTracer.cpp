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

#include "PathStoreTracer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Types/ValidationResult.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class PathStoreImpl>
PathStoreTracer<PathStoreImpl>::PathStoreTracer(arangodb::ResourceMonitor& resourceMonitor)
    : _impl{resourceMonitor} {}

template <class PathStoreImpl>
PathStoreTracer<PathStoreImpl>::~PathStoreTracer() {
  LOG_TOPIC("f39e8", INFO, Logger::GRAPHS) << "PathStore Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("f39e9", INFO, Logger::GRAPHS) << "  " << name << ": " << trace;
  }
}

template <class PathStoreImpl>
void PathStoreTracer<PathStoreImpl>::reset() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["reset"].addTiming(TRI_microtime() - start));
  return _impl.reset();
}

template <class PathStoreImpl>
size_t PathStoreTracer<PathStoreImpl>::append(Step step) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["append"].addTiming(TRI_microtime() - start));
  return _impl.append(step);
}

template <class PathStoreImpl>
size_t PathStoreTracer<PathStoreImpl>::size() const {
  double start = TRI_microtime();
  TRI_DEFER(_stats["size"].addTiming(TRI_microtime() - start));
  return _impl.size();
}

template <class PathStoreImpl>
template <class ProviderType>
auto PathStoreTracer<PathStoreImpl>::buildPath(Step const& vertex,
                                               PathResult<ProviderType, Step>& path) const
    -> void {
  double start = TRI_microtime();
  TRI_DEFER(_stats["buildPath"].addTiming(TRI_microtime() - start));
  _impl.buildPath(vertex, path);
}

template <class PathStoreImpl>
template <class ProviderType>
auto PathStoreTracer<PathStoreImpl>::reverseBuildPath(Step const& vertex,
                                                      PathResult<ProviderType, Step>& path) const
    -> void {
  double start = TRI_microtime();
  TRI_DEFER(_stats["reverseBuildPath"].addTiming(TRI_microtime() - start));
  _impl.reverseBuildPath(vertex, path);
}

template <class PathStoreImpl>
auto PathStoreTracer<PathStoreImpl>::visitReversePath(
    const Step& step, const std::function<bool(const Step&)>& visitor) const -> bool {
  double start = TRI_microtime();
  TRI_DEFER(_stats["visitReversePath"].addTiming(TRI_microtime() - start));
  return _impl.visitReversePath(step, visitor);
}

/* SingleServerProvider Section */

template class ::arangodb::graph::PathStoreTracer<PathStore<SingleServerProvider::Step>>;

// Tracing
template void ::arangodb::graph::PathStoreTracer<PathStore<SingleServerProvider::Step>>::buildPath<ProviderTracer<SingleServerProvider>>(
    ProviderTracer<SingleServerProvider>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider>, ProviderTracer<SingleServerProvider>::Step>& path) const;

template void arangodb::graph::PathStoreTracer<PathStore<SingleServerProvider::Step>>::reverseBuildPath<ProviderTracer<SingleServerProvider>>(
    ProviderTracer<SingleServerProvider>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider>, ProviderTracer<SingleServerProvider>::Step>& path) const;

/* ClusterProvider Section */

template class ::arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>;

// Tracing
template void ::arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>::buildPath<ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;

template void arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>::reverseBuildPath<ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;
