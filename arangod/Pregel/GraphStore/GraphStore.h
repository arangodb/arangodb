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

#include <DocumentID.h>

namespace arangodb::pregel {

template<typename Attributes>
struct Vertex {
  DocumentId id;
  Attributes attributes;
};

template<typename Attributes>
struct Edge {
  // This is currently never used, as we don't write back to edges (or lazily load data after the fact).
  // tere should be some way in the future to use it.
  // DocumentID id;
 DocumentId to;
  Attributes attributes;
};

template<typename VertexAttributes, typename EdgeAttributes>
struct GraphStore {
  using V = Vertex<VertexAttributes>;
  using E = Edge<EdgeAttributes>;

  std::vector<V> vertices;
  std::unordered_map<DocumentId, std::vector<E>> edges;
};

}  // namespace arangodb::pregel
