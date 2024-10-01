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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"

#include <numeric>

namespace arangodb {

namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {

template<class ProviderType, class Step>
class PathResult {
  using VertexRef = arangodb::velocypack::HashedStringRef;

 public:
  enum WeightType { NONE, AMOUNT_EDGES, ACTUAL_WEIGHT };

  PathResult(ProviderType& sourceProvider, ProviderType& targetProvider);
  auto clear() -> void;
  auto appendVertex(typename Step::Vertex v) -> void;
  auto prependVertex(typename Step::Vertex v) -> void;
  auto appendEdge(typename Step::Edge e, double weight) -> void;
  auto prependEdge(typename Step::Edge e, double weight) -> void;
  auto addWeight(double weight) -> void;
  auto toVelocyPack(arangodb::velocypack::Builder& builder,
                    WeightType addWeight = WeightType::NONE) -> void;
  auto isEqualEdgeRepresentation(PathResult<ProviderType, Step> const& other)
      -> bool;
  auto lastVertexToVelocyPack(arangodb::velocypack::Builder& builder) -> void;
  auto lastEdgeToVelocyPack(arangodb::velocypack::Builder& builder) -> void;

  auto isEmpty() const -> bool;
  auto getWeight() const -> double { return _pathWeight; }
  auto getWeight(size_t which) -> double { return _weights[which]; }

  auto getLength() const -> size_t { return _edges.size(); }
  auto getVertex(size_t which) const -> Step::Vertex {
    return _vertices[which];
  }
  auto getEdge(size_t which) const -> Step::Edge { return _edges[which]; }
  auto getWeight(size_t which) const -> double { return _weights[which]; }
  auto getSourceProvider() const -> ProviderType& { return _sourceProvider; }
  auto getTargetProvider() const -> ProviderType& { return _targetProvider; }

  // The following is an approximation and takes the actual vectors below
  // into account as well as the object itself. It intentionally does not
  // count the referenced strings, since these are accounted for elsewhere!
  auto getMemoryUsage() const -> size_t;

  // The following implements a total order on the set of path results,
  // which has the property that paths of larger weight are considered
  // to be less than paths of smaller weight. This can be used to make
  // PathResults unique and sort them in a descending fashion by weight.
  auto compare(PathResult<ProviderType, Step> const& other)
      -> std::strong_ordering;

 private:
  std::vector<typename Step::Vertex> _vertices;
  std::vector<typename Step::Edge> _edges;
  std::vector<double> _weights;

  // The number of vertices delivered by the source provider in the vector.
  // We need to load this amount of vertices from source, all others from target
  // For edges we need to load one edge less from here.
  size_t _numVerticesFromSourceProvider;
  size_t _numEdgesFromSourceProvider;
  double _pathWeight;

  // Provider for the beginning of the path (source)
  ProviderType& _sourceProvider;
  // Provider for the end of the path (target)
  ProviderType& _targetProvider;
};
}  // namespace graph
}  // namespace arangodb
