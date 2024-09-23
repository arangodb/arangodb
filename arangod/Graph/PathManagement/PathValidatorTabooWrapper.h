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

#pragma once

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
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

// This template class serves as a wrapper for other PathValidator types.
// It essentially hands on everything and adds capabilities to define some
// forbidden vertices and edges which are to be ignored in the graph.
template<class PathValidatorImplementation>
class PathValidatorTabooWrapper {
 public:
  using VertexRef = arangodb::velocypack::HashedStringRef;
  using Edge =
      typename PathValidatorImplementation::ProviderImpl::Step::EdgeType;
  using ProviderImpl = typename PathValidatorImplementation::ProviderImpl;
  using Step = typename ProviderImpl::Step;
  using PathStoreImpl = typename PathValidatorImplementation::PathStoreImpl;
  using VertexSet =
      arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                    std::equal_to<VertexRef>>;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  PathValidatorTabooWrapper(ProviderImpl& provider, PathStoreImpl& store,
                            PathValidatorOptions opts);
  ~PathValidatorTabooWrapper();

  auto validatePath(typename PathStoreImpl::Step& step) -> ValidationResult;
  auto validatePath(
      typename PathStoreImpl::Step const& step,
      PathValidatorTabooWrapper<PathValidatorImplementation> const&
          otherValidator) -> ValidationResult;
  auto validatePathWithoutGlobalVertexUniqueness(
      typename PathStoreImpl::Step& step) -> ValidationResult;

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

  void setForbiddenVertices(std::shared_ptr<VertexSet> forbidden) {
    _forbiddenVertices = std::move(forbidden);
  }

  void setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden) {
    _forbiddenEdges = std::move(forbidden);
  }

 private:
  PathValidatorImplementation _impl;
  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable containers::FlatHashMap<std::string, TraceEntry> _stats;

  std::shared_ptr<VertexSet> _forbiddenVertices;
  std::shared_ptr<EdgeSet> _forbiddenEdges;
};
}  // namespace graph
}  // namespace arangodb
