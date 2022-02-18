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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "Graph.h"
#include "Basics/debugging.h"

namespace arangodb::pregel3 {

template<DerivedFromBaseGraphProperties A, DerivedFromBaseVertexProperties B,
         DerivedFromSimulateUndirectedEdgeProperties C>
void UndirectableGraph<A, B, C>::makeUndirected() {
  for (auto source = 0; source < this->numVertices(); ++source) {
    for (auto const& target : this->vertexProperties.at(source).neighbors) {
      auto idxOfSource = neighborsIdx(target, source);
      if (idxOfSource != -1) {
        addEdge(target, source, idxOfSource);
      }
    }
  }
}
template<DerivedFromBaseGraphProperties GP, DerivedFromBaseVertexProperties VP,
         DerivedFromSimulateUndirectedEdgeProperties EP>
ssize_t UndirectableGraph<GP, VP, EP>::addEdge(size_t source, size_t target,
                                               ssize_t idx, bool ensureSingle,
                                               bool origin) {
  idx = Graph<GP, VP, EP>::addEdge(source, target, idx, ensureSingle);
  if (idx != -1) {
    this->edgeProperties[idx].origin = origin;
  }
  return idx;
}

template<DerivedFromBaseGraphProperties G, DerivedFromBaseVertexProperties V,
         DerivedFromBaseEdgeProperties E>
ssize_t Graph<G, V, E>::addEdge(size_t source, size_t target, ssize_t idx,
                                bool ensureSingle) {
  auto idxOfTarget = neighborsIdx(source, target);
  if (ensureSingle && idxOfTarget != -1) {
    return -1;
  }
  if (idx == -1) {
    idx = edgeProperties.size();
    edgeProperties.emplace_back();
  }
  vertexProperties[source].neighbors.push_back(target);
  vertexProperties[source].outEdges[idxOfTarget].push_back(idx);
  return idx;
}

}  // namespace arangodb::pregel3