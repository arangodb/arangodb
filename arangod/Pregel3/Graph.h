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

#include <vector>

#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Containers/FlatHashMap.h"

/**
 * Requirements to a graph representation
 *
 * 1. Possibly memory efficient
 * 2. O(1) for
 *  (2.1) get the neighbor properties given by its id
 *  (2.2) get the outgoing/incident edge properties given by its id
 *  (2.3) given an outgoing (incident) edge by its id, get the head/the other
 * end (2.4) given a neighbor, get the list of all edges between them
 * 3. Iterate over neighbors in linear time wrt. their number
 * 4. Iterate over outgoing/incident edges in linear time wrt. their number
 */

/// Every vertex has a unique id. The ids start from 0 and end with n-1 where n
/// is the number of vertices in the graph. The same for edges.

namespace arangodb::pregel3 {
class BaseGraphProperties {
 public:
  bool isDirected;
};

class BaseVertexProperties {
 public:
  explicit BaseVertexProperties(LocalDocumentId const& token)
      : localDocumentId(token) {}
  // For directed graphs, the indexes of the out-neighbors.
  // For undirected graphs, the indexes of all neighbors.
  std::vector<size_t> neighbors;
  // Map a vertex index (in graph's vertices) of a neighbor to the position
  // of the neighbor in neighbors.
  std::unordered_map<size_t, size_t> neighborsReverse;
  // For directed graphs, the indexes of the outgoing edges.
  // For undirected graphs, the indexes of all incident edges.
  // The order of neighbors is the order of the corresponding edges.
  std::vector<std::vector<size_t>> outEdges;
  // A helper class to find an edge given by its index in graph's edges
  // in the list of outgoing/incident edges
  struct IncidentEdgePosition {
    size_t posInNeighbors;
    size_t posAmongParallel;
  };
  std::unordered_map<size_t, IncidentEdgePosition> egdesReverse;
  // identify the vertex in the graph
  arangodb::LocalDocumentId const& localDocumentId;
};

class BaseEdgeProperties {
  // The edge does not know its endpoints, only its properties like the weight.
};

class SimulateUndirectedEdgeProperties : public BaseEdgeProperties {
 public:
  bool original = true;
};

class MinCutEdgeProps : public SimulateUndirectedEdgeProperties {
 public:
  double capacity = 1.0;
  double preflow = 0.0;
  // double residualCapacity = 1.0;
};

struct MinCutVertexProps : BaseVertexProperties {
  double excess = 0.0;
  size_t label = 0;
  bool leaf = false;
};

template<class EdgeProperties>
concept DerivedFromSimulateUndirectedEdgeProperties =
    std::is_base_of_v<SimulateUndirectedEdgeProperties, EdgeProperties> ||
    std::is_same_v<SimulateUndirectedEdgeProperties, EdgeProperties>;

template<class EdgeProperties>
concept DerivedFromMinCutEdgeProps =
    std::is_base_of_v<MinCutEdgeProps, EdgeProperties> ||
    std::is_same_v<MinCutEdgeProps, EdgeProperties>;

template<class VertexProperties>
concept DerivedFromMinCutVertexProps =
    std::is_base_of_v<MinCutVertexProps, VertexProperties> ||
    std::is_same_v<MinCutVertexProps, VertexProperties>;

template<class GraphProperties>
concept DerivedFromBaseGraphProperties =
    std::is_base_of_v<BaseGraphProperties, GraphProperties> ||
    std::is_same_v<BaseGraphProperties, GraphProperties>;

template<class EdgeProperties>
concept DerivedFromBaseEdgeProperties =
    std::is_base_of_v<BaseEdgeProperties, EdgeProperties> ||
    std::is_same_v<BaseEdgeProperties, EdgeProperties>;

template<class VertexProperties>
concept DerivedFromBaseVertexProperties =
    std::is_base_of_v<BaseVertexProperties, VertexProperties> ||
    std::is_same_v<BaseVertexProperties, VertexProperties>;

//////////   GRAPH  //////////
template<DerivedFromBaseGraphProperties GraphProperties,
         DerivedFromBaseVertexProperties VertexProperties,
         DerivedFromBaseEdgeProperties EdgeProperties>
class Graph {
 public:
  using GraphProps = GraphProperties;
  using VertexProps = VertexProperties;
  using EdgeProps = EdgeProperties;

  Graph() = default;
  size_t numVertices() const { return vertexProperties.size(); }
  size_t numEdges() const { return edgeProperties.size(); }

  // todo: make this O(1)
  /**
   * For vertices source and target return the index of target in the list of
   * neighbors of source. If target is not a neighbor of source, return -1.
   * @param source
   * @param target
   * @return
   */
  ssize_t neighborsIdx(size_t source, size_t target) {
    auto it = vertexProperties.at(source).neighborsReverse.find(target);
    if (it == vertexProperties.at(source).neighborsReverse.end()) {
      return -1;
    }
    return it.second;
  }

  bool isEdge(size_t source, size_t target) {
    return neighborsIdx(source, target) != -1;
  }

  ssize_t addEdge(size_t source, size_t target, ssize_t idx = -1,
                  bool ensureSingle = false);

  std::pair<size_t, size_t> invertEdge(size_t u, size_t idxNeighbor) const {
    size_t const otherVertex = vertexProperties[u].neighbors.at(idxNeighbor);
    size_t const uIdx = vertexProperties[otherVertex].neighborsReverse.at(u);
    return std::make_pair(otherVertex, uIdx);
  }

  // get all edges between vertex u and its idxNeighb-th neighbor
  std::vector<size_t>& getEdgesNeighb(size_t u, size_t idxNeighb) {
    return vertexProperties.at(u).outEdges.at(idxNeighb);
  }

  // get all edges (const) between vertex u and its idxNeighb-th neighbor
  std::vector<size_t>& getEdgesNeighb(size_t u, size_t idxNeighb) const {
    return vertexProperties.at(u).outEdges.at(idxNeighb);
  }

  bool isDirected() const { return graphProperties.isDirected; }

  std::vector<VertexProperties> vertexProperties;
  std::vector<EdgeProperties> edgeProperties;
  GraphProperties graphProperties;
  // maps a vertex _id (in the db) to its index in vertexProperties
  containers::FlatHashMap<std::string, size_t> vertexIdToIdx;
  // the reverse
  std::vector<std::string> idxToVertexId;
};

using BaseGraph =
    Graph<BaseGraphProperties, BaseVertexProperties, BaseEdgeProperties>;

template<DerivedFromBaseGraphProperties GP, DerivedFromBaseVertexProperties VP,
         DerivedFromSimulateUndirectedEdgeProperties EP>
class UndirectableGraph : public Graph<GP, VP, EP> {
 public:
  /**
   * Insert for the edge (a, b) which has the lowest index (in edgeProperties)
   * among all edges (a, b) the edge (b, a) if the graph does not already
   * have any edge (b, a). The inserted edge has the same properties as the edge
   * (a, b).
   */
  void makeUndirected();
  ssize_t addEdge(size_t source, size_t target, ssize_t idx = -1,
                  bool ensureSingle = false, bool origin = true);
};

class MinCutGraph
    : public UndirectableGraph<BaseGraphProperties, MinCutVertexProps,
                               MinCutEdgeProps> {
 public:
  bool activeVertex(size_t v) const { return vertexProperties[v].excess > 0; }

  /**
   * Number of neighbors (not number of edges!).
   * @param v
   * @return
   */
  size_t outDegree(size_t v) const {
    return vertexProperties.at(v).neighbors.size();
  }

  double residual(size_t u, size_t v) {
    return vertexProperties[u].label == vertexProperties[u].label + 1;
  }
};

}  // namespace arangodb::pregel3

// parallelise load graph + measure what takes how much time
// mincut in new pregel