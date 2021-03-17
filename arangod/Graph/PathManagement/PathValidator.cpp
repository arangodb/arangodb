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
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Types/ValidationResult.h"

#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
PathValidator<PathStore, vertexUniqueness>::PathValidator(PathStore const& store)
    : _store(store) {}

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<PathStore, vertexUniqueness>::validatePath(typename PathStore::Step const& step)
    -> ValidationResult {
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
  return ValidationResult{ValidationResult::Type::TAKE};
}

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<PathStore, vertexUniqueness>::validatePath(
    typename PathStore::Step const& step,
    PathValidator<PathStore, vertexUniqueness> const& otherValidator) -> ValidationResult {
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

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<PathStore, vertexUniqueness>::exposeUniqueVertices() const
    -> ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>, std::equal_to<VertexRef>> const& {
  return _uniqueVertices;
}

namespace arangodb {
namespace graph {

/* SingleServerProvider Section */
template class PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::PATH>;
template class PathValidator<PathStoreTracer<PathStore<SingleServerProvider::Step>>, VertexUniquenessLevel::PATH>;

/* ClusterProvider Section */
template class PathValidator<PathStore<ClusterProvider::Step>, VertexUniquenessLevel::PATH>;
template class PathValidator<PathStoreTracer<PathStore<ClusterProvider::Step>>, VertexUniquenessLevel::PATH>;

}  // namespace graph
}  // namespace arangodb
