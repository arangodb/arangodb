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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Pregel/GraphStore/Edge.h"
#include "Pregel/GraphStore/Vertex.h"

namespace arangodb::pregel {

/*
 * A quiver stores a bit of a graph; currently it stores vertex-centric: it
 * stores vertices together with outgoing edges;
 * the reason for this is mostly for backwards-compatibility with other
 * Pregel code, and might change in future
 */
template<typename V, typename E>
struct Quiver {
  using VertexType = Vertex<V, E>;
  using EdgeType = Edge<E>;

  auto emplace(VertexType&& v) -> void { vertices.emplace_back(std::move(v)); }
  auto numberOfVertices() -> size_t { return vertices.size(); }
  // TODO
  auto numberOfEdges() -> size_t { return 0; }

  auto begin() { return std::begin(vertices); }
  auto end() { return std::end(vertices); }

  std::vector<VertexType> vertices;
};

template<typename V, typename E, typename Inspector>
auto inspect(Inspector& f, Quiver<V, E>& s) {
  return f.object(s).fields(f.field("vertices", s.vertices));
}

}  // namespace arangodb::pregel
