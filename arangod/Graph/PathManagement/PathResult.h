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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H
#define ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H 1

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"

#include <numeric>

namespace arangodb {

namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {

template <class ProviderType, class Step>
class PathResult {
  using VertexRef = arangodb::velocypack::HashedStringRef;

 public:
  PathResult(ProviderType& sourceProvider, ProviderType& targetProvider);
  auto clear() -> void;
  auto appendVertex(typename Step::Vertex v) -> void;
  auto prependVertex(typename Step::Vertex v) -> void;
  auto appendEdge(typename Step::Edge e) -> void;
  auto prependEdge(typename Step::Edge e) -> void;
  auto toVelocyPack(arangodb::velocypack::Builder& builder) -> void;
  
  auto isEmpty() const -> bool;

 private:
  std::vector<typename Step::Vertex> _vertices;
  std::vector<typename Step::Edge> _edges;

  // The number of vertices delivered by the source provider in the vector.
  // We need to load this amount of vertices from source, all others from target
  // For edges we need to load one edge less from here.
  size_t _numVerticesFromSourceProvider;
  size_t _numEdgesFromSourceProvider;
  
  // Provider for the beginning of the path (source)
  ProviderType& _sourceProvider;
  // Provider for the end of the path (target)
  ProviderType& _targetProvider;
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H
