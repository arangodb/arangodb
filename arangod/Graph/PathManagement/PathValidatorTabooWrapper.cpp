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

#include "PathValidator.h"
#include "PathValidatorTabooWrapper.h"
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

using namespace arangodb;
using namespace arangodb::graph;

template<class PathValidatorImplementation>
PathValidatorTabooWrapper<PathValidatorImplementation>::
    PathValidatorTabooWrapper(ProviderImpl& provider, PathStoreImpl& store,
                              PathValidatorOptions opts)
    : _impl{provider, store, std::move(opts)} {}

template<class PathValidatorImplementation>
PathValidatorTabooWrapper<
    PathValidatorImplementation>::~PathValidatorTabooWrapper(){};

template<class PathValidatorImplementation>
auto PathValidatorTabooWrapper<PathValidatorImplementation>::validatePath(
    typename PathStoreImpl::Step& step) -> ValidationResult {
  auto v = step.getVertex().getID();
  if (_forbiddenVertices->contains(step.getVertex().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  if (_forbiddenEdges->contains(step.getEdge().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  return _impl.validatePath(step);
}

template<class PathValidatorImplementation>
auto PathValidatorTabooWrapper<PathValidatorImplementation>::validatePath(
    typename PathStoreImpl::Step const& step,
    PathValidatorTabooWrapper<PathValidatorImplementation> const&
        otherValidator) -> ValidationResult {
  if (_forbiddenVertices->contains(step.getVertex().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  if (_forbiddenEdges->contains(step.getEdge().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  return _impl.validatePath(step, otherValidator._impl);
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<PathValidatorImplementation>::reset() {
  return _impl.reset();
}

template<class PathValidatorImplementation>
bool PathValidatorTabooWrapper<PathValidatorImplementation>::usesPrune() const {
  return true;  // We do prune if we hit forbidden vertices or edges!
}

template<class PathValidatorImplementation>
bool PathValidatorTabooWrapper<PathValidatorImplementation>::usesPostFilter()
    const {
  return _impl.usesPostFilter();
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<PathValidatorImplementation>::setPruneContext(
    aql::InputAqlItemRow& inputRow) {
  return _impl.setPruneContext(inputRow);
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<PathValidatorImplementation>::
    setPostFilterContext(aql::InputAqlItemRow& inputRow) {
  return _impl.setPostFilterContext(inputRow);
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<
    PathValidatorImplementation>::unpreparePruneContext() {
  return _impl.unpreparePruneContext();
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<
    PathValidatorImplementation>::unpreparePostFilterContext() {
  return _impl.unpreparePostFilterContext();
}

namespace arangodb::graph {
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

#ifdef USE_ENTERPRISE
using SmartGraphStep = ::arangodb::graph::enterprise::SmartGraphStep;
#endif

template class PathValidatorTabooWrapper<PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

#ifdef USE_ENTERPRISE
template class PathValidatorTabooWrapper<PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
#endif

/* ClusterProvider Section */
template class PathValidatorTabooWrapper<
    PathValidator<ClusterProvider<ClusterProviderStep>,
                  PathStore<ClusterProvider<ClusterProviderStep>::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<PathValidator<
    ProviderTracer<ClusterProvider<ClusterProviderStep>>,
    PathStoreTracer<
        PathStore<ProviderTracer<ClusterProvider<ClusterProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

#ifdef USE_ENTERPRISE
template class PathValidatorTabooWrapper<PathValidator<
    enterprise::SmartGraphProvider<ClusterProviderStep>,
    PathStore<enterprise::SmartGraphProvider<ClusterProviderStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<PathValidator<
    ProviderTracer<enterprise::SmartGraphProvider<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        enterprise::SmartGraphProvider<ClusterProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
#endif

}  // namespace arangodb::graph
