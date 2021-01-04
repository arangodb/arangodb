////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Types/ValidationResult.h"

#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
PathValidator<PathStore, vertexUniqueness>::PathValidator(PathStore const& store)
    : _store(store) {}

template <class PathStore, VertexUniquenessLevel vertexUniqueness>
auto PathValidator<PathStore, vertexUniqueness>::track(typename PathStore::Step const& step)
    -> void {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

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

namespace arangodb {
namespace graph {
template class PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::GLOBAL>;
template class PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::PATH>;
template class PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::NONE>;
}
}  // namespace arangodb