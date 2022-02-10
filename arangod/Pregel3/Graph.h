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
  // For directed graphs, the indexes of the outgoing edges.
  // For undirected graphs, the indexes of all incident edges.
  // The order of neighbors is the order of the corresponding edges.
  std::vector<std::vector<size_t>> outEdges;
  // identify the vertex in the graph
  arangodb::LocalDocumentId const& localDocumentId;
};

class BaseEdgeProperties {
  // The edge does not know its endpoints, only its properties like the weight.
};

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

  std::vector<VertexProperties> vertexProperties;
  std::vector<EdgeProperties> edgeProperties;
  GraphProperties graphProperties;
  // maps a vertex _id to its index in vertexProperties
  containers::FlatHashMap<std::string, size_t> vertexIdToIdx;
  std::vector<std::string> idxToVertexId;
};

using BaseGraph =
    Graph<BaseGraphProperties, BaseVertexProperties, BaseEdgeProperties>;
}  // namespace arangodb::pregel3

// parallelise load graph + measure what takes how much time
// mincut in new pregel