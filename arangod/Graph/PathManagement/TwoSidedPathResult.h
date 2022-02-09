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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/HashedStringRef.h>
#include "Containers/HashSet.h"
#include "Graph/Enumerators/OneSidedEnumeratorInterface.h"

#include <numeric>

namespace arangodb {

namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {

template<class ProviderType, class Step>
class TwoSidedPathResult : public PathResultInterface {
  using VertexRef = arangodb::velocypack::HashedStringRef;

 public:
  TwoSidedPathResult(ProviderType& sourceProvider,
                     ProviderType& targetProvider);
  auto clear() -> void;
  auto appendVertex(typename Step::Vertex v) -> void;
  auto prependVertex(typename Step::Vertex v) -> void;
  auto appendEdge(typename Step::Edge e) -> void;
  auto prependEdge(typename Step::Edge e) -> void;
  auto toVelocyPack(arangodb::velocypack::Builder& builder) -> void override;
  auto lastVertexToVelocyPack(arangodb::velocypack::Builder& builder)
      -> void override;
  auto lastEdgeToVelocyPack(arangodb::velocypack::Builder& builder)
      -> void override;

  [[nodiscard]] auto isEmpty() const -> bool;

 private:
  // vertices collected by extending the path from a middle vertex to the source
  // The order of vertices in the vector is inverse to the order in the path
  // (i.e., the source will be the last vertex in _sourceVertices).
  std::vector<typename Step::Vertex> _sourceVertices;
  // vertices collected by extending the path from the target
  // The order of vertices in the vector is the order in the path
  // (i.e., the target will be the last vertex in _sourceVertices).
  std::vector<typename Step::Vertex> _targetVertices;
  // edges collected by extending the path from the source
  std::vector<typename Step::Edge> _sourceEdges;
  // edges collected by extending the path from the target
  std::vector<typename Step::Edge> _targetEdges;

  // Provider for the beginning of the path (source)
  ProviderType& _sourceProvider;
  // Provider for the end of the path (target)
  ProviderType& _targetProvider;
};
}  // namespace graph
}  // namespace arangodb
