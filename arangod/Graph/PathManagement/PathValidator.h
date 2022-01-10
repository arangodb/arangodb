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

#pragma once

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
#include "Graph/Types/UniquenessLevel.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {
class PruneExpressionEvaluator;
}
namespace graph {

class ValidationResult;

/*
 * TODO:
 *
 * - EdgeUniqueness, like vertex Uniqueness, and sometimes Edge and Vertex
 * Uniqueness enforce each other. (e.g. => VertexUniqueness == PATH =>
 * EdgeUniquess PATH || NONE.
 *
 * - Prune Condition.
 * - PostFilter Condition.
 * - Path Conditions. Vertex, Edge maybe in LookupInfo
 *     (e.g. p.vertices[*].name ALL == "HANS")
 */
template<class Provider, class PathStore,
         VertexUniquenessLevel vertexUniqueness>
class PathValidator {
  using VertexRef = arangodb::velocypack::HashedStringRef;

 public:
  PathValidator(Provider& provider, PathStore const& store,
                PathValidatorOptions opts);
  ~PathValidator();

  auto validatePath(typename PathStore::Step const& step) -> ValidationResult;
  auto validatePath(typename PathStore::Step const& step,
                    PathValidator<Provider, PathStore, vertexUniqueness> const&
                        otherValidator) -> ValidationResult;

  void setPruneEvaluator(std::unique_ptr<aql::PruneExpressionEvaluator> eval);

  void setPostFilterEvaluator(
      std::unique_ptr<aql::PruneExpressionEvaluator> eval);

  void reset();

 private:
  PathStore const& _store;
  Provider& _provider;

  // Only for applied vertex uniqueness
  // TODO: Figure out if we can make this Member template dependend
  //       e.g. std::enable_if<vertexUniqueness != NONE>
  ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                  std::equal_to<VertexRef>>
      _uniqueVertices;

  PathValidatorOptions _options;

  std::unique_ptr<aql::PruneExpressionEvaluator> _pruneEvaluator;

  std::unique_ptr<aql::PruneExpressionEvaluator> _postFilterEvaluator;

  arangodb::velocypack::Builder _tmpObjectBuilder;

 private:
  auto evaluateVertexCondition(typename PathStore::Step const&)
      -> ValidationResult;
  auto evaluateVertexRestriction(typename PathStore::Step const& step) -> bool;

  auto exposeUniqueVertices() const
      -> ::arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                         std::equal_to<VertexRef>> const&;

  auto evaluateVertexExpression(arangodb::aql::Expression* expression,
                                arangodb::velocypack::Slice value) -> bool;
};
}  // namespace graph
}  // namespace arangodb
