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
#include "Graph/Types/ValidationResult.h"

#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness>::PathValidator(
    ProviderType& provider, PathStore const& store, PathValidatorOptions opts)
    : _store(store), _provider(provider), _options(std::move(opts)) {}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
PathValidator<ProviderType, PathStore, vertexUniqueness>::~PathValidator() = default;

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::validatePath(
    typename PathStore::Step const& step) -> ValidationResult {
  auto res = evaluateEdgeCondition(step);
  if (res.isFiltered() && res.isPruned()) {
    // Can give up here. This Value is not used
    return res;
  }

  if constexpr (vertexUniqueness == VertexUniquenessLevel::PATH) {
    _uniqueVertices.clear();
    // Reserving here is pointless, we will test paths that increase by at most 1 entry.

    bool success =
        _store.visitReversePath(step, [&](typename PathStore::Step const& step) -> bool {
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
    auto const& [unused, added] = _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!added) {
      res.combine(ValidationResult::Type::FILTER);
    }
  }
  return res;
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::validatePath(
    typename PathStore::Step const& step,
    PathValidator<ProviderType, PathStore, vertexUniqueness> const& otherValidator)
    -> ValidationResult {
  if constexpr (vertexUniqueness == VertexUniquenessLevel::PATH) {
    // For PATH: take _uniqueVertices of otherValidator, and run Visitor of other side, check if one vertex is duplicate.
    auto const& otherUniqueVertices = otherValidator.exposeUniqueVertices();

    bool success =
        _store.visitReversePath(step, [&](typename PathStore::Step const& innerStep) -> bool {
          // compare memory address for equality (instead of comparing their values)
          if (&step == &innerStep) {
            return true;
          }

          // If otherUniqueVertices has our step, we will return false and abort.
          // Otherwise we'll return true here.
          // This guarantees we have no vertex on both sides of the path twice.
          return otherUniqueVertices.find(innerStep.getVertexIdentifier()) ==
                 otherUniqueVertices.end();
        });
    if (!success) {
      return ValidationResult{ValidationResult::Type::FILTER};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }
  if constexpr (vertexUniqueness == VertexUniquenessLevel::GLOBAL) {
    auto const& [unused, added] = _uniqueVertices.emplace(step.getVertexIdentifier());
    // If this add fails, we need to exclude this path
    if (!added) {
      return ValidationResult{ValidationResult::Type::FILTER};
    }
    return ValidationResult{ValidationResult::Type::TAKE};
  }

  // For NONE: ignoreOtherValidator return TAKE
  return ValidationResult{ValidationResult::Type::TAKE};
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::exposeUniqueVertices() const
    -> ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>, std::equal_to<VertexRef>> const& {
  return _uniqueVertices;
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::evaluateEdgeCondition(
    typename PathStore::Step const& step) -> ValidationResult {
  if (step.isFirst()) {
    // First step never has an edge, nothing to be checked here
    return ValidationResult{ValidationResult::Type::TAKE};
  }
  auto expr = _options.getEdgeExpression(step.getDepth());
  if (expr != nullptr) {
    // TODO: Maybe we want to replace this by ExpressionContext for simplicity

    // By Convention, the previous value has to be on the Last Member.
    // And has to be on the Left Side as a String node
    // Nowwe have to inject the vertex value.
    auto node = expr->nodeForModification();

    TRI_ASSERT(node->numMembers() > 0);
    auto dirCmp = node->getMemberUnchecked(node->numMembers() - 1);
    TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);
    TRI_ASSERT(dirCmp->numMembers() == 2);

    auto idNode = dirCmp->getMemberUnchecked(1);
    TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
    TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
    auto prev = _store.get(step.getPrevious());
    auto vertexId = prev.getVertexIdentifier();
    idNode->setStringValue(vertexId.data(), vertexId.length());
    _tmpObjectBuilder.clear();
    _provider.addEdgeToBuilder(step.getEdge(), _tmpObjectBuilder);
    bool satifiesCondition = evaluateExpression(expr, _tmpObjectBuilder.slice());
    if (!satifiesCondition) {
      return ValidationResult{ValidationResult::Type::FILTER};
    }
  }
  return ValidationResult{ValidationResult::Type::TAKE};
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<ProviderType, PathStore, vertexUniqueness>::evaluateExpression(
    arangodb::aql::Expression* expression, VPackSlice value) -> bool {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(value.isObject() || value.isNull());
  auto tmpVar = _options.getTempVar();
  expression->setVariable(tmpVar, value);
  TRI_DEFER(expression->clearVariable(tmpVar));
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(_options.getExpressionContext(), mustDestroy);
  aql::AqlValueGuard guard{res, mustDestroy};
  TRI_ASSERT(res.isBoolean());
  return res.toBoolean();
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness>::setPruneEvaluator(
    std::unique_ptr<aql::PruneExpressionEvaluator> eval) {
  _pruneEvaluator = std::move(eval);
}

template <class ProviderType, class PathStore, VertexUniquenessLevel vertexUniqueness>
void PathValidator<ProviderType, PathStore, vertexUniqueness>::setPostFilterEvaluator(
    std::unique_ptr<aql::PruneExpressionEvaluator> eval) {
  _postFilterEvaluator = std::move(eval);
}

namespace arangodb {
namespace graph {

/* SingleServerProvider Section */
template class PathValidator<SingleServerProvider, PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::NONE>;
template class PathValidator<ProviderTracer<SingleServerProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<SingleServerProvider>::Step>>, VertexUniquenessLevel::NONE>;

template class PathValidator<SingleServerProvider, PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::PATH>;
template class PathValidator<ProviderTracer<SingleServerProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<SingleServerProvider>::Step>>, VertexUniquenessLevel::PATH>;

template class PathValidator<SingleServerProvider, PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::GLOBAL>;
template class PathValidator<ProviderTracer<SingleServerProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<SingleServerProvider>::Step>>, VertexUniquenessLevel::GLOBAL>;

/* ClusterProvider Section */
template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>, VertexUniquenessLevel::NONE>;
template class PathValidator<ProviderTracer<ClusterProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>, VertexUniquenessLevel::NONE>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>, VertexUniquenessLevel::PATH>;
template class PathValidator<ProviderTracer<ClusterProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>, VertexUniquenessLevel::PATH>;

template class PathValidator<ClusterProvider, PathStore<ClusterProvider::Step>, VertexUniquenessLevel::GLOBAL>;
template class PathValidator<ProviderTracer<ClusterProvider>,
                             PathStoreTracer<PathStore<ProviderTracer<ClusterProvider>::Step>>, VertexUniquenessLevel::GLOBAL>;

}  // namespace graph
}  // namespace arangodb
