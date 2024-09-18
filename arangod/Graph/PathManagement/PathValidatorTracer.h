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

#pragma once

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
#include "Graph/Types/ForbiddenVertices.h"
#include "Graph/Types/UniquenessLevel.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Helpers/TraceEntry.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {
class PruneExpressionEvaluator;
class InputAqlItemRow;
}  // namespace aql

namespace graph {

class ValidationResult;

template<class PathValidatorImplementation>
class PathValidatorTracer {
 public:
  using VertexRef = arangodb::velocypack::HashedStringRef;
  using VertexSet =
      arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                    std::equal_to<VertexRef>>;
  using Provider = typename PathValidatorImplementation::ProviderImpl;
  using Step = typename Provider::Step;
  using PathStore = typename PathValidatorImplementation::PathStoreImpl;
  using Edge = typename Step::EdgeType;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  PathValidatorTracer(Provider& provider, PathStore& store,
                      PathValidatorOptions opts);
  ~PathValidatorTracer();

  auto validatePath(typename PathStore::Step& step) -> ValidationResult;
  auto validatePath(
      typename PathStore::Step const& step,
      PathValidatorTracer<PathValidatorImplementation> const& otherValidator)
      -> ValidationResult;
  auto validatePathWithoutGlobalVertexUniqueness(typename PathStore::Step& step)
      -> ValidationResult;

  void reset();

  // Prune section
  bool usesPrune() const;
  void setPruneContext(aql::InputAqlItemRow& inputRow);

  // Post filter section
  bool usesPostFilter() const;
  void setPostFilterContext(aql::InputAqlItemRow& inputRow);

  /**
   * @brief In case prune or postFilter has been enabled, we need to unprepare
   * the context of the inputRow. See explanation in TraversalExecutor.cpp L200
   */
  void unpreparePruneContext();
  void unpreparePostFilterContext();

  auto setForbiddenVertices(std::shared_ptr<VertexSet> forbidden)
      -> void requires HasForbidden<PathValidatorImplementation> {
    _impl.setForbiddenVertices(std::move(forbidden));
  };

  auto setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden)
      -> void requires HasForbidden<PathValidatorImplementation> {
    _impl.setForbiddenEdges(std::move(forbidden));
  };

 private : PathValidatorImplementation _impl;
  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable containers::FlatHashMap<std::string, TraceEntry> _stats;
};
}  // namespace graph
}  // namespace arangodb
