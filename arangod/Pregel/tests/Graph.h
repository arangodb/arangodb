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

#pragma once

#include <map>
#include <unordered_set>

#include <Basics/VelocyPackStringLiteral.h>

#include "DisjointSet.h"

using namespace arangodb::velocypack;

struct EmptyEdgeProperties {};
struct EmptyVertexProperties {};

using VertexKey = std::string;
using EdgeKey = std::string;

template<typename VertexProperties = EmptyVertexProperties>
struct BaseVertex {
  VertexKey _key;
  VertexProperties properties;

  auto operator==(BaseVertex const& other) { return _key == other._key; }
};

template<typename VertexProperties>
concept VertexPropertiesWithValue = requires(VertexProperties t) {
                                      {
                                        t.value
                                        } -> std::convertible_to<size_t>;
                                    };

template<typename EdgeProperties = EmptyEdgeProperties>
struct BaseEdge {
  EdgeKey _key;
  VertexKey _from;
  VertexKey _to;
  EdgeProperties properties;
};

using VertexIndex = size_t;
using EdgeIndex = size_t;

template<typename EdgeProperties, typename VertexProperties>
class Graph {
 public:
  using Vertex = BaseVertex<VertexProperties>;
  using Edge = BaseEdge<EdgeProperties>;

  Graph() = default;

  auto readVertices(SharedSlice const& graphJson, bool checkDuplicateVertices)
      -> void;

  auto readVertices(std::istream& file, bool checkDuplicateVertices)
      -> void;

  size_t readEdges(SharedSlice const& graphJson, bool checkEdgeEnds,
                   std::function<void(Edge edge)> onReadEdge);

  size_t readEdges(std::istream& file, bool checkEdgeEnds,
                   std::function<void(Edge edge)> onReadEdge);

  auto getVertexPosition(VertexKey const& key) -> VertexIndex {
    return _vertexPosition[key];
  }
  auto clearVertexPositions() -> void { _vertexPosition.clear(); }

  std::vector<Vertex> vertices;

 private:
  std::map<VertexKey, VertexIndex> _vertexPosition;

 protected:
  auto _checkEdgeEnds(Edge, std::map<VertexKey, size_t> const& vertexPosition)
      -> void;
  void checkSliceAddVertex(Slice vertexSlice, size_t count,
                           bool checkDuplicateVertices);
  void checkSliceAddEdge(Slice slice, bool checkEdgeEnds,
                         std::function<void(Edge)>& onReadEdge);
};

/**
   * Write
   * Compute the weakly connected components. Each component has an id of type
   * size_t and is written into the value property of each of its vertices.
   * @return the number of weakly connected components.
 */
template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
class ValuedGraph : public Graph<EdgeProperties, VertexProperties> {
 public:
  size_t writeEquivalenceRelationIntoVertices(DisjointSet& eqRel) {
    // todo: if a lot of (small) connected components, it may be better to use a
    //  bitset
    std::unordered_set<size_t> markedRepresentatives;
    size_t counter = 0;
    for (size_t i = 0; i < this->vertices.size(); ++i) {
      size_t const representative = eqRel.representative(i);
      if (markedRepresentatives.contains(representative)) {
        this->vertices[i].properties.value =
            this->vertices[representative].properties.value;
      } else {
        size_t const newId = counter++;
        this->vertices[i].properties.value = newId;
        this->vertices[representative].properties.value = newId;
        markedRepresentatives.insert(representative);
      }
    }
    return counter;
  };
};

template<typename Inspector, typename VertexProperties>
auto inspect(Inspector& f, BaseVertex<VertexProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key),
                            f.embedFields(p.properties));
}

template<typename Inspector>
auto inspect(Inspector& f, EmptyEdgeProperties& x) {
  return f.object(x).fields();
}

template<typename Inspector>
auto inspect(Inspector& f, EmptyVertexProperties& x) {
  return f.object(x).fields();
}


template<typename Inspector, typename EdgeProperties = EmptyEdgeProperties>
auto inspect(Inspector& f, BaseEdge<EdgeProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key), f.field("_from", p._from),
                            f.field("_to", p._to), f.embedFields(p.properties));
}
