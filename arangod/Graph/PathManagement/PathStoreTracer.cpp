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
#include "Graph/PathManagement/SingleProviderPathResult.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

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
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["reset"].addTiming(TRI_microtime() - start); });
  return _impl.reset();
}

template <class PathStoreImpl>
size_t PathStoreTracer<PathStoreImpl>::append(Step step) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["append"].addTiming(TRI_microtime() - start); });
  return _impl.append(step);
}

template <class PathStoreImpl>
typename PathStoreImpl::Step PathStoreTracer<PathStoreImpl>::getStep(size_t position) const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["getStep"].addTiming(TRI_microtime() - start); });
  return _impl.getStep(position);
}

template <class PathStoreImpl>
typename PathStoreImpl::Step& PathStoreTracer<PathStoreImpl>::getStepReference(size_t position) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["getStepReference"].addTiming(TRI_microtime() - start); });
  return _impl.getStepReference(position);
}

template <class PathStoreImpl>
size_t PathStoreTracer<PathStoreImpl>::size() const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["size"].addTiming(TRI_microtime() - start); });
  return _impl.size();
}

template <class PathStoreImpl>
template <class PathResultType>
auto PathStoreTracer<PathStoreImpl>::buildPath(Step const& vertex, PathResultType& path) const
    -> void {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["buildPath"].addTiming(TRI_microtime() - start); });
  _impl.buildPath(vertex, path);
}

template <class PathStoreImpl>
template <class ProviderType>
auto PathStoreTracer<PathStoreImpl>::reverseBuildPath(Step const& vertex,
                                                      PathResult<ProviderType, Step>& path) const
    -> void {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["reverseBuildPath"].addTiming(TRI_microtime() - start); });
  _impl.reverseBuildPath(vertex, path);
}

template <class PathStoreImpl>
auto PathStoreTracer<PathStoreImpl>::visitReversePath(
    const Step& step, const std::function<bool(const Step&)>& visitor) const -> bool {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["visitReversePath"].addTiming(TRI_microtime() - start); });
  return _impl.visitReversePath(step, visitor);
}

template <class PathStoreImpl>
auto PathStoreTracer<PathStoreImpl>::modifyReversePath(Step& step,
                                                       const std::function<bool(Step&)>& visitor)
    -> bool {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept { _stats["modifyReversePath"].addTiming(TRI_microtime() - start); });
  return _impl.modifyReversePath(step, visitor);
}

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::PathStoreTracer<PathStore<SingleServerProviderStep>>;

// Tracing
template void ::arangodb::graph::PathStoreTracer<PathStore<SingleServerProviderStep>>::buildPath<
    PathResult<ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
               ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>(
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
               ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>& path) const;

template void arangodb::graph::PathStoreTracer<PathStore<SingleServerProviderStep>>::reverseBuildPath<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>>(
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
               ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>& path) const;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::PathStoreTracer<PathStore<enterprise::SmartGraphStep>>;

template void ::arangodb::graph::PathStoreTracer<PathStore<enterprise::SmartGraphStep>>::buildPath<
    PathResult<ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>>(
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>& path) const;

template void arangodb::graph::PathStoreTracer<PathStore<enterprise::SmartGraphStep>>::reverseBuildPath<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>>(
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step const& vertex,
    PathResult<ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
        ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>::Step>& path) const;
#endif

/* ClusterProvider Section */

template class ::arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>;

// Tracing
template void ::arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>::buildPath<
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;

template void arangodb::graph::PathStoreTracer<PathStore<ClusterProvider::Step>>::reverseBuildPath<ProviderTracer<ClusterProvider>>(
    ProviderTracer<ClusterProvider>::Step const& vertex,
    PathResult<ProviderTracer<ClusterProvider>, ProviderTracer<ClusterProvider>::Step>& path) const;
