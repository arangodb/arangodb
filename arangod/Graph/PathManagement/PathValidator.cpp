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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PathValidator.h"
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

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness,
              edgeUniqueness>::PathValidator(ProviderType& provider,
                                             PathStore& store,
                                             PathValidatorOptions opts)
    : _store(store), _provider(provider), _options(std::move(opts)) {}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness,
              edgeUniqueness>::~PathValidator() = default;

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    validatePath(typename PathStore::Step const& step) -> ValidationResult {
  auto ctx = _options.getExpressionContext();
  // Reset variables
  ctx.clearVariableValues();
  auto res = evaluateVertexCondition(step);
  if (res.isFiltered() && res.isPruned()) {
    // Can give up here. This Value is not used
    return res;
  }

  if constexpr (vertexUniqueness == VertexUniquenessLevel::PATH) {
    _uniqueVertices.clear();
    _uniqueEdges.clear();
    // Reserving here is pointless, we will test paths that increase by at most
    // 1 entry.

    bool success = _store.visitReversePath(
        step, [&](typename PathStore::Step const& step) -> bool {
          auto const& [unusedV, addedVertex] =
              _uniqueVertices.emplace(step.getVertexIdentifier());

          if (!step.isFirst()) {
            auto const& [unusedE, addedEdge] =
                _uniqueEdges.emplace(step.getEdgeIdentifier());
            // If this add fails, we need to exclude this path
            return addedVertex && addedEdge;
          }

          // If this add fails, we need to exclude this path
          return addedVertex;
        });
    if (!success) {
      res.combine(ValidationResult::Type::FILTER_AND_PRUNE);
    }
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::GLOBAL) {
    // In case we have VertexUniquenessLevel::GLOBAL, we do not have to take
    // care about the EdgeUniquenessLevel.
    auto const& [unusedV, addedVertex] =
        _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!addedVertex) {
      res.combine(ValidationResult::Type::FILTER_AND_PRUNE);
    }
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::NONE &&
                edgeUniqueness == EdgeUniquenessLevel::PATH) {
    _uniqueEdges.clear();

    if (!step.isFirst()) {
      bool edgeSuccess = _store.visitReversePath(
          step, [&](typename PathStore::Step const& step) -> bool {
            auto const& [unusedE, addedEdge] =
                _uniqueEdges.emplace(step.getEdgeIdentifier());
            // If this add fails, we need to exclude this path
            return addedEdge;
          });

      // If this add fails, we need to exclude this path
      if (!edgeSuccess) {
        res.combine(ValidationResult::Type::FILTER_AND_PRUNE);
      }
    }
  }
  return res;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    validatePath(typename PathStore::Step const& step,
                 PathValidator<ProviderType, PathStore, vertexUniqueness,
                               edgeUniqueness> const& otherValidator)
        -> ValidationResult {
  if constexpr (vertexUniqueness == VertexUniquenessLevel::PATH) {
    // For PATH: take _uniqueVertices of otherValidator, and run Visitor of
    // other side, check if one vertex is duplicate.
    auto const& otherUniqueVertices = otherValidator.exposeUniqueVertices();

    bool success = _store.visitReversePath(
        step, [&](typename PathStore::Step const& innerStep) -> bool {
          // compare memory address for equality (instead of comparing their
          // values)
          if (&step == &innerStep) {
            return true;
          }

          // If otherUniqueVertices has our step, we will return false and
          // abort. Otherwise we'll return true here. This guarantees we have no
          // vertex on both sides of the path twice.
          return otherUniqueVertices.find(innerStep.getVertexIdentifier()) ==
                 otherUniqueVertices.end();
        });
    if (!success) {
      return ValidationResult{ValidationResult::Type::FILTER_AND_PRUNE};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::GLOBAL) {
    auto const& [unused, added] =
        _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!added) {
      return ValidationResult{ValidationResult::Type::FILTER_AND_PRUNE};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }

  // For NONE: ignoreOtherValidator return TAKE
  return ValidationResult{ValidationResult::Type::TAKE};
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::exposeUniqueVertices() const
    -> ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                       std::equal_to<VertexRef>> const& {
  return _uniqueVertices;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    evaluateVertexRestriction(typename PathStore::Step const& step) -> bool {
  if (step.isFirst()) {
    // first step => always allowed
    return true;
  }

  auto const& allowedCollections = _options.getAllowedVertexCollections();
  if (allowedCollections.empty()) {
    // all allowed
    return true;
  }

  auto collectionName = step.getCollectionName();
  if (std::find(allowedCollections.begin(), allowedCollections.end(),
                collectionName) != allowedCollections.end()) {
    // found in allowed collections => allowed
    return true;
  }

  // not allowed
  return false;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    evaluateVertexCondition(typename PathStore::Step const& step)
        -> ValidationResult {
  // evaluate if vertex collection is allowed
  bool isAllowed = evaluateVertexRestriction(step);
  if (!isAllowed) {
    if (_options.bfsResultHasToIncludeFirstVertex() && step.isFirst()) {
      return ValidationResult{ValidationResult::Type::PRUNE};
    }
    return ValidationResult{ValidationResult::Type::FILTER_AND_PRUNE};
  }

  // evaluate if vertex needs to be pruned
  ValidationResult res{ValidationResult::Type::TAKE};
  if (_options.usesPrune()) {
    // TODO [GraphRefactor]: Possible performance optimization. Please double
    // check if we can do better then using three different types of builders
    VPackBuilder vertexBuilder, edgeBuilder, pathBuilder;

    auto& evaluator = _options.getPruneEvaluator();

    if (evaluator->needsVertex()) {
      _provider.addVertexToBuilder(step.getVertex(), vertexBuilder);
      evaluator->injectVertex(vertexBuilder.slice());
    }
    if (evaluator->needsEdge()) {
      _provider.addEdgeToBuilder(step.getEdge(), edgeBuilder);
      evaluator->injectEdge(edgeBuilder.slice());
    }
    if (evaluator->needsPath()) {
      // TODO [GraphRefactor]: Improve this section. Currently, I do not like
      // the design here. I. Drop of PathStore const& qualifier was necessary to
      // initialize PathResultInterface. II. I don't want to distinguish between
      // different ProviderTypes here if possible (best case).
      if (std::is_same_v<ProviderType,
                         SingleServerProvider<SingleServerProviderStep>>) {
        using ResultPathType =
            SingleProviderPathResult<ProviderType, PathStore, Step>;
        std::unique_ptr<PathResultInterface> currentPath =
            std::make_unique<ResultPathType>(step, _provider, _store);
        currentPath->toVelocyPack(pathBuilder);
        evaluator->injectPath(pathBuilder.slice());
      } else {
        PathResult<ProviderType, Step> currentPath{_provider, _provider};
        _store.buildPath(step, currentPath);
        currentPath.toVelocyPack(pathBuilder);
        evaluator->injectPath(pathBuilder.slice());
        // Currently unused
        TRI_ASSERT(false);
      }
    }
    if (evaluator->evaluate()) {
      res.combine(ValidationResult::Type::PRUNE);
    }
  }

  if (res.isPruned() && res.isFiltered()) {
    return res;
  }

  // Evaluate depth-based vertex expressions
  auto vertexExpr = _options.getVertexExpression(step.getDepth());
  if (vertexExpr != nullptr) {
    _tmpObjectBuilder.clear();
    _provider.addVertexToBuilder(step.getVertex(), _tmpObjectBuilder);

    // evaluate expression
    bool satifiesCondition =
        evaluateVertexExpression(vertexExpr, _tmpObjectBuilder.slice());
    if (!satifiesCondition) {
      if (_options.bfsResultHasToIncludeFirstVertex() && step.isFirst()) {
        return ValidationResult{ValidationResult::Type::PRUNE};
      }
      return ValidationResult{ValidationResult::Type::FILTER_AND_PRUNE};
    }
  }

  if (_options.usesPostFilter()) {
    VPackBuilder vertexBuilder, edgeBuilder;

    auto& evaluator = _options.getPostFilterEvaluator();

    if (evaluator->needsVertex()) {
      _provider.addVertexToBuilder(step.getVertex(), vertexBuilder);
      evaluator->injectVertex(vertexBuilder.slice());
    }
    if (evaluator->needsEdge()) {
      _provider.addEdgeToBuilder(step.getEdge(), edgeBuilder);
      evaluator->injectEdge(edgeBuilder.slice());
    }

    TRI_ASSERT(!evaluator->needsPath());
    if (!evaluator->evaluate()) {
      if (!_options.bfsResultHasToIncludeFirstVertex() || !step.isFirst()) {
        res.combine(ValidationResult::Type::FILTER);
      }
    }
  }

  return res;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    evaluateVertexExpression(arangodb::aql::Expression* expression,
                             VPackSlice value) -> bool {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(value.isObject() || value.isNull());
  auto tmpVar = _options.getTempVar();
  bool mustDestroy = false;
  auto ctx = _options.getExpressionContext();
  aql::AqlValue tmpVal{value};
  ctx.setVariableValue(tmpVar, tmpVal);
  aql::AqlValue res = expression->execute(&ctx, mustDestroy);
  aql::AqlValueGuard guard{res, mustDestroy};
  TRI_ASSERT(res.isBoolean());
  return res.toBoolean();
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::reset() {
  if constexpr (vertexUniqueness != VertexUniquenessLevel::NONE) {
    _uniqueVertices.clear();
  }

  if constexpr (edgeUniqueness != EdgeUniquenessLevel::NONE) {
    _uniqueEdges.clear();
  }
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
bool PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::usesPrune() const {
  if (_options.usesPrune()) {
    return true;
  }
  return false;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
bool PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::usesPostFilter() const {
  if (_options.usesPostFilter()) {
    return true;
  }
  return false;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    setPruneContext(aql::InputAqlItemRow& inputRow) {
  TRI_ASSERT(_options.usesPrune());
  _options.setPruneContext(inputRow);
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness, edgeUniqueness>::
    setPostFilterContext(aql::InputAqlItemRow& inputRow) {
  TRI_ASSERT(_options.usesPostFilter());
  _options.setPostFilterContext(inputRow);
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::unpreparePruneContext() {
  TRI_ASSERT(_options.usesPrune());
  _options.unpreparePruneContext();
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness,
                   edgeUniqueness>::unpreparePostFilterContext() {
  TRI_ASSERT(_options.usesPostFilter());
  _options.unpreparePostFilterContext();
}

namespace arangodb::graph {

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class PathValidator<SingleServerProvider<SingleServerProviderStep>,
                             PathStore<SingleServerProviderStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>;
template class PathValidator<SingleServerProvider<SingleServerProviderStep>,
                             PathStore<SingleServerProviderStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>;

#ifdef USE_ENTERPRISE
template class PathValidator<SingleServerProvider<enterprise::SmartGraphStep>,
                             PathStore<enterprise::SmartGraphStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>;
template class PathValidator<SingleServerProvider<enterprise::SmartGraphStep>,
                             PathStore<enterprise::SmartGraphStep>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>;
#endif

/* ClusterProvider Section */
template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>;
template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::NONE,
                             EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::NONE, EdgeUniquenessLevel::PATH>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::PATH,
                             EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::GLOBAL,
                             EdgeUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>;

}  // namespace arangodb::graph
