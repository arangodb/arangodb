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
////////////////////////////////////////////////////////////////////////////////

#include "PathValidator.h"
#include "PathValidatorTracer.h"
#include "Aql/AstNode.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/PathManagement/SingleProviderPathResult.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"

using namespace arangodb;
using namespace arangodb::graph;

template<class PathValidatorImplementation>
PathValidatorTracer<PathValidatorImplementation>::PathValidatorTracer(
    Provider& provider, PathStore& store, PathValidatorOptions opts)
    : _impl{provider, store, std::move(opts)} {}

template<class PathValidatorImplementation>
PathValidatorTracer<PathValidatorImplementation>::~PathValidatorTracer() {
  LOG_TOPIC("3b86e", INFO, Logger::GRAPHS) << "PathValidator Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("hjg79", INFO, Logger::GRAPHS) << " " << name << ": " << trace;
  }
};

template<class PathValidatorImplementation>
auto PathValidatorTracer<PathValidatorImplementation>::validatePath(
    typename PathStore::Step const& step) -> ValidationResult {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["validatePath"].addTiming(TRI_microtime() - start);
  });
  return _impl.validatePath(step);
}

template<class PathValidatorImplementation>
auto PathValidatorTracer<PathValidatorImplementation>::validatePath(
    typename PathStore::Step const& step,
    PathValidatorTracer<PathValidatorImplementation> const& otherValidator)
    -> ValidationResult {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["validatePath"].addTiming(TRI_microtime() - start);
  });
  return _impl.validatePath(step, otherValidator._impl);
}

template<class PathValidatorImplementation>
void PathValidatorTracer<PathValidatorImplementation>::reset() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["reset"].addTiming(TRI_microtime() - start); });
  return _impl.reset();
}

template<class PathValidatorImplementation>
bool PathValidatorTracer<PathValidatorImplementation>::usesPrune() const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["usesPrune"].addTiming(TRI_microtime() - start);
  });
  return _impl.usesPrune();
}

template<class PathValidatorImplementation>
bool PathValidatorTracer<PathValidatorImplementation>::usesPostFilter() const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["usesPostFilter"].addTiming(TRI_microtime() - start);
  });
  return _impl.usesPostFilter();
}

template<class PathValidatorImplementation>
void PathValidatorTracer<PathValidatorImplementation>::setPruneContext(
    aql::InputAqlItemRow& inputRow) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["setPruneContext"].addTiming(TRI_microtime() - start);
  });
  return _impl.setPruneContext(inputRow);
}

template<class PathValidatorImplementation>
void PathValidatorTracer<PathValidatorImplementation>::setPostFilterContext(
    aql::InputAqlItemRow& inputRow) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["setPostFilterContext"].addTiming(TRI_microtime() - start);
  });
  return _impl.setPostFilterContext(inputRow);
}

template<class PathValidatorImplementation>
void PathValidatorTracer<PathValidatorImplementation>::unpreparePruneContext() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["unpreparePruneContext"].addTiming(TRI_microtime() - start);
  });
  return _impl.unpreparePruneContext();
}

template<class PathValidatorImplementation>
void PathValidatorTracer<
    PathValidatorImplementation>::unpreparePostFilterContext() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["unpreparePostFilterContext"].addTiming(TRI_microtime() - start);
  });
  return _impl.unpreparePostFilterContext();
}


namespace arangodb::graph {
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;
using SmartGraphStep =  ::arangodb::graph::enterprise::SmartGraphStep;

template class PathValidatorTracer<
    PathValidator<SingleServerProvider<SingleServerProviderStep>,
                  PathStore<SingleServerProviderStep>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;


template class PathValidatorTracer<PathValidator<SingleServerProvider<SingleServerProviderStep>,
                             PathStore<SingleServerProviderStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;



#ifdef USE_ENTERPRISE
template class PathValidatorTracer<PathValidator<SingleServerProvider<enterprise::SmartGraphStep>,
                             PathStore<enterprise::SmartGraphStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::NONE>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;


template class PathValidatorTracer<PathValidator<SingleServerProvider<enterprise::SmartGraphStep>,
                             PathStore<enterprise::SmartGraphStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;


#endif

/* ClusterProvider Section */
template class PathValidatorTracer<PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::NONE>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;


template class PathValidatorTracer<PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::PATH,
                             EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;



template class PathValidatorTracer<PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::GLOBAL,
                             EdgeUniquenessLevel::PATH>>;


template class PathValidatorTracer<PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;



}  // namespace arangodb::graph
