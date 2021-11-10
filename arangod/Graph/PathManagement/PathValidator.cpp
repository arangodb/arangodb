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

#include "Aql/AstNode.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness>::PathValidator(
    ProviderType& provider, PathStore const& store, PathValidatorOptions opts)
    : _store(store), _provider(provider), _options(std::move(opts)) {}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness>::~PathValidator() =
    default;

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::validatePath(
    typename PathStore::Step const& step) -> ValidationResult {
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
    // Reserving here is pointless, we will test paths that increase by at most
    // 1 entry.

    bool success = _store.visitReversePath(
        step, [&](typename PathStore::Step const& step) -> bool {
          auto const& [unused, added] =
              _uniqueVertices.emplace(step.getVertexIdentifier());
          // If this add fails, we need to exclude this path
          return added;
        });
    if (!success) {
      res.combine(ValidationResult::Type::FILTER);
    }
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::GLOBAL) {
    auto const& [unused, added] =
        _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!added) {
      res.combine(ValidationResult::Type::FILTER);
    }
  }
  return res;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::validatePath(
    typename PathStore::Step const& step,
    PathValidator<ProviderType, PathStore, vertexUniqueness> const&
        otherValidator) -> ValidationResult {
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
      return ValidationResult{ValidationResult::Type::FILTER};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::GLOBAL) {
    auto const& [unused, added] =
        _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!added) {
      return ValidationResult{ValidationResult::Type::FILTER};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }

  // For NONE: ignoreOtherValidator return TAKE
  return ValidationResult{ValidationResult::Type::TAKE};
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore,
                   vertexUniqueness>::exposeUniqueVertices() const
    -> ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                       std::equal_to<VertexRef>> const& {
  return _uniqueVertices;
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::
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
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::
    evaluateVertexCondition(typename PathStore::Step const& step)
        -> ValidationResult {
  // evaluate if vertex collection is allowed
  bool isAllowed = evaluateVertexRestriction(step);
  if (!isAllowed) {
    if (_options.hasCompatibility38IncludeFirstVertex() && step.isFirst()) {
      return ValidationResult{ValidationResult::Type::PRUNE};
    }
    return ValidationResult{ValidationResult::Type::FILTER};
  }

  auto expr = _options.getVertexExpression(step.getDepth());
  if (expr != nullptr) {
    _tmpObjectBuilder.clear();
    _provider.addVertexToBuilder(step.getVertex(), _tmpObjectBuilder);

    // evaluate expression
    bool satifiesCondition =
        evaluateVertexExpression(expr, _tmpObjectBuilder.slice());
    if (!satifiesCondition) {
      if (_options.hasCompatibility38IncludeFirstVertex() && step.isFirst()) {
        return ValidationResult{ValidationResult::Type::PRUNE};
      }
      return ValidationResult{ValidationResult::Type::FILTER};
    }
  }
  return ValidationResult{ValidationResult::Type::TAKE};
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::
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
         VertexUniquenessLevel vertexUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness>::
    setPruneEvaluator(std::unique_ptr<aql::PruneExpressionEvaluator> eval) {
  _pruneEvaluator = std::move(eval);
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness>::
    setPostFilterEvaluator(
        std::unique_ptr<aql::PruneExpressionEvaluator> eval) {
  _postFilterEvaluator = std::move(eval);
}

template<class ProviderType, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness>::reset() {
  if constexpr (vertexUniqueness != VertexUniquenessLevel::NONE) {
    _uniqueVertices.clear();
  }
}

namespace arangodb {
namespace graph {

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class PathValidator<SingleServerProvider<SingleServerProviderStep>,
                             PathStore<SingleServerProviderStep>,
                             VertexUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::NONE>;

template class PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<SingleServerProviderStep>,
    PathStore<SingleServerProvider<SingleServerProviderStep>::Step>,
    VertexUniquenessLevel::GLOBAL>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<
        ProviderTracer<SingleServerProvider<SingleServerProviderStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL>;

#ifdef USE_ENTERPRISE
template class PathValidator<SingleServerProvider<enterprise::SmartGraphStep>,
                             PathStore<enterprise::SmartGraphStep>,
                             VertexUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::NONE>;

template class PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::PATH>;

template class PathValidator<
    SingleServerProvider<enterprise::SmartGraphStep>,
    PathStore<SingleServerProvider<enterprise::SmartGraphStep>::Step>,
    VertexUniquenessLevel::GLOBAL>;
template class PathValidator<
    ProviderTracer<SingleServerProvider<enterprise::SmartGraphStep>>,
    PathStoreTracer<PathStore<ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>::Step>>,
    VertexUniquenessLevel::GLOBAL>;
#endif

/* ClusterProvider Section */
template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::NONE>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::NONE>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::PATH>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::PATH>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>,
                             VertexUniquenessLevel::GLOBAL>;
template class PathValidator<
    ProviderTracer<ClusterProvider>,
    PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>,
    VertexUniquenessLevel::GLOBAL>;

}  // namespace graph
}  // namespace arangodb
