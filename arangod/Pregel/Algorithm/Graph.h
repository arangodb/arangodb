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

namespace arangodb::pregel::graph {

using VertexKey = std::string;
using EdgeKey = std::string;

template<typename VertexProperties>
struct Vertex {
  VertexKey _key;
  VertexProperties properties;
};

template<typename Inspector, typename VertexProperties>
auto inspect(Inspector& f, Vertex<VertexProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key),
                            f.embedFields(p.properties));
}

template<typename EdgeProperties>
struct Edge {
  EdgeKey _key;
  VertexKey _from;
  VertexKey _to;
  EdgeProperties properties;
};

template<typename Inspector, typename EdgeProperties>
auto inspect(Inspector& f, Edge<EdgeProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key), f.field("_from", p._from),
                            f.field("_to", p._to), f.embedFields(p.properties));
}

template<typename VertexProperties, typename EdgeProperties>
struct Graph {
  std::vector<Vertex<VertexProperties>> vertices;
  std::vector<Edge<EdgeProperties>> edges;
};

}  // namespace arangodb::pregel::graph
