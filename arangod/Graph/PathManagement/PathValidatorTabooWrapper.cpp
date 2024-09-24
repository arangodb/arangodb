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
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#endif

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"

#include "Logger/LogMacros.h"

namespace arangodb::graph {

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
  if (_forbiddenVertices != nullptr &&
      _forbiddenVertices->contains(step.getVertex().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  if (_forbiddenEdges != nullptr &&
      _forbiddenEdges->contains(step.getEdge().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  return _impl.validatePath(step);
}

template<class PathValidatorImplementation>
auto PathValidatorTabooWrapper<PathValidatorImplementation>::validatePath(
    typename PathStoreImpl::Step const& step,
    PathValidatorTabooWrapper<PathValidatorImplementation> const&
        otherValidator) -> ValidationResult {
  if (_forbiddenVertices != nullptr &&
      _forbiddenVertices->contains(step.getVertex().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  if (_forbiddenEdges != nullptr &&
      _forbiddenEdges->contains(step.getEdge().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  return _impl.validatePath(step, otherValidator._impl);
}

template<class PathValidatorImplementation>
auto PathValidatorTabooWrapper<PathValidatorImplementation>::
    validatePathWithoutGlobalVertexUniqueness(
        typename PathStoreImpl::Step& step) -> ValidationResult {
  auto v = step.getVertex().getID();
  if (_forbiddenVertices != nullptr &&
      _forbiddenVertices->contains(step.getVertex().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  if (_forbiddenEdges != nullptr &&
      _forbiddenEdges->contains(step.getEdge().getID())) {
    return ValidationResult(ValidationResult::Type::FILTER_AND_PRUNE);
  }
  return _impl.validatePathWithoutGlobalVertexUniqueness(step);
}

template<class PathValidatorImplementation>
void PathValidatorTabooWrapper<PathValidatorImplementation>::reset() {
  _forbiddenVertices.reset();
  _forbiddenEdges.reset();
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

// Explicit template instantiations:

using SingleProvider = SingleServerProvider<SingleServerProviderStep>;

template class PathValidatorTabooWrapper<
    PathValidator<SingleProvider, PathStore<SingleServerProviderStep>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<
    PathValidator<ProviderTracer<SingleProvider>,
                  PathStoreTracer<PathStore<SingleServerProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

/* ClusterProvider Section */

using ClustProvider = ClusterProvider<ClusterProviderStep>;

template class PathValidatorTabooWrapper<
    PathValidator<ClustProvider, PathStore<ClusterProviderStep>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

template class PathValidatorTabooWrapper<
    PathValidator<ProviderTracer<ClustProvider>,
                  PathStoreTracer<PathStore<ClusterProviderStep>>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

}  // namespace arangodb::graph
