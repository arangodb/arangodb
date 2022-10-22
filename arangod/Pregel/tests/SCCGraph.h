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

#include <unordered_set>
#include <queue>

#include "Graph.h"

using namespace arangodb::velocypack;

using VertexIndex = size_t;
using EdgeIndex = size_t;

struct SCCVertexProperties {
  uint64_t value{};
  VertexIndex treeParent{};
};

template<typename Inspector>
auto inspect(Inspector& f, SCCVertexProperties& x) {
  return f.object(x).fields(f.field("value", x.value));
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
class SCCGraph : public ValuedGraph<EdgeProperties, VertexProperties> {
 public:
  using SCCEdge = BaseEdge<EdgeProperties>;

  enum class EdgeType {
    BACKWARD,
    CROSS_FORWARD,
    CROSS_NON_FORWARD,
    FORWARD_OR_SELF_LOOP
  };

  SCCGraph(SharedSlice const& graphJson, bool checkDuplicateVertices);

  DisjointSet sccs;
  auto readEdgesBuildSCCs(SharedSlice const& graphJson) -> void;

 private:
  std::queue<std::pair<VertexIndex, VertexIndex>>
      _currentStream;  // S_i from the paper
  std::queue<std::pair<VertexIndex, VertexIndex>>
      _nextStream;  // S_{i+1} from the paper
  VertexIndex const _idxDummyRoot;

  bool changedTree = true;
  /**
   * Executes the body from the main algorithm loop on the given edge from the
   * initial queue (S_0 from the paper) given in the graph input string and
   * fills _nextQueue as S_{i+1} from the paper.
   * @param edge
   */
  auto onReadEdge(SCCEdge const& edge) -> void;
  auto processEdge(
      VertexIndex from, VertexIndex to,
      std::queue<std::pair<VertexIndex, VertexIndex>>& outputQueue) -> void;
  auto getTreeParent(VertexIndex idx) -> VertexIndex& {
    return this->vertices[idx].properties.treeParent;
  };
  auto edgeType(VertexIndex from, VertexIndex to) -> EdgeType;
  auto collapse(VertexIndex from, VertexIndex to) -> void;
};
