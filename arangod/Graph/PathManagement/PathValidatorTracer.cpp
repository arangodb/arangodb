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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PathValidator.h"
#include "PathValidatorTabooWrapper.h"
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
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#endif

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"

#include "Logger/LogMacros.h"

namespace arangodb::graph {

template<class PathValidatorImplementation>
PathValidatorTracer<PathValidatorImplementation>::PathValidatorTracer(
    Provider& provider, PathStore& store, PathValidatorOptions opts)
    : _impl{provider, store, std::move(opts)} {}

template<class PathValidatorImplementation>
PathValidatorTracer<PathValidatorImplementation>::~PathValidatorTracer() {
  LOG_TOPIC("3b86e", INFO, Logger::GRAPHS) << "PathValidator Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("a7a84", INFO, Logger::GRAPHS) << " " << name << ": " << trace;
  }
};

template<class PathValidatorImplementation>
auto PathValidatorTracer<PathValidatorImplementation>::validatePath(
    typename PathStore::Step& step) -> ValidationResult {
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
auto PathValidatorTracer<PathValidatorImplementation>::
    validatePathWithoutGlobalVertexUniqueness(typename PathStore::Step& step)
        -> ValidationResult {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["validatePath"].addTiming(TRI_microtime() - start);
  });
  return _impl.validatePathWithoutGlobalVertexUniqueness(step);
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

//////////////////////////////////////////////////////////////////////////
// Explicit template instanciations:
//
// This template is used in various places, since it wraps the "normal"
// PathValidator as well as the PathValidatorTabooWrapper which is needed
// for Yen's algorithm. For all cases, we need a SingleServer and a Cluster
// case. For the vertex and edge uniqueness, we need the following
// combinations for the OneSidedEnumerator (i.e. Traversals):
//    VertexUniqueness    EdgeUniqueness
//    NONE                NONE
//    NONE                PATH
//    PATH                PATH
//    GLOBAL              PATH
// Note that the combinations PATH/NONE and GLOBAL/NONE would make sense
// but are not used, since they produce the same outcome as PATH/PATH and
// GLOBAL/PATH respectively.
// The TwoSidedEnumerator only uses the following combinations:
//    VertexUniqueness    EdgeUniqueness
//    PATH                PATH
//    GLOBAL              PATH
// For the PathValidatorTabooWrapper, we only need the latter one, since
// it is only used in the YenEnumerator.
// Furthermore, we only need the PathValidatorTracer in cases where all
// template parameters of the PathValidator use tracing, too.
// For the enterprise version, we need SingleServerProvider<SmartGraphStep>
// and SmartGraphProvider<ClusterProviderStep>, but only for the
// OneSidedEnumerator, since no logic for the TwoSidedEnumerator and
// smart graphs has been implemented yet.
//
// This is the information which lead to the selection of the concrete
// template instanciations below.
//////////////////////////////////////////////////////////////////////////

using SingleProvider = SingleServerProvider<SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
using SmartGraphStep = ::arangodb::graph::enterprise::SmartGraphStep;
#endif

template class PathValidatorTracer<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidatorTabooWrapper<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>>;

#ifdef USE_ENTERPRISE

template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<enterprise::SmartGraphStep>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<enterprise::SmartGraphStep>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<enterprise::SmartGraphStep>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<enterprise::SmartGraphStep>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

#endif

/* ClusterProvider Section */

using ClustProvider = ClusterProvider<ClusterProviderStep>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidatorTabooWrapper<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>>;

#ifdef USE_ENTERPRISE

template class PathValidatorTracer<PathValidator<
    ProviderTracer<enterprise::SmartGraphProvider<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ClusterProviderStep>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<enterprise::SmartGraphProvider<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ClusterProviderStep>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<enterprise::SmartGraphProvider<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ClusterProviderStep>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTracer<PathValidator<
    ProviderTracer<enterprise::SmartGraphProvider<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ClusterProviderStep>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

#endif

}  // namespace arangodb::graph
