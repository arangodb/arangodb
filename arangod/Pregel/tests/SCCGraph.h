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

/**
 * The class is intended solely to compute SCCs. This is on-the-fly while
 * reading graph edges. The algorithm is taken from the paper
 * Laura, L., Santaroni, F. (2011). Computing Strongly Connected Components in
 * the Streaming Model. In: Marchetti-Spaccamela, A., Segal, M. (eds) Theory and
 * Practice of Algorithms in (Computer) Systems. TAPAS 2011. Lecture Notes in
 * Computer Science, vol 6595. Springer, Berlin, Heidelberg.
 * https://doi.org/10.1007/978-3-642-19754-3_20
 *
 * Description of the algorithm.
 *
 * The algorithm uses two data structures: UnionFind and a tree whose nodes are
 * vertices that represent SCCs computed so far. The graph vertex that
 * serves as a node in the tree is contained in the SCC it represents but is not
 * necessarily the representative of the SCC which is returned for the SCC by
 * the UnionFind.
 *
 * Globally, the run of the algorithm consists of a series of edge stream reads.
 * The first edge stream comes from the input. It may produce the next stream,
 * which is read and may produce the next stream and so on. While reading edges
 * from a stream, the tree is updated. In some cases it signals that we found a
 * cycle whose vertices are added to the corresponding SCC. The algorithm stops
 * when for a stream there were no tree updates (even if there is a remaining
 * unread stream!).
 *
 * Initially, the vertices are read and put into UnionFind as singletons and
 * into the tree as children of the auxiliary root. When an edge is read from a
 * stream, we determine its type with respect to the tree: backward, forward or
 * self-loop, cross forward or cross non-forward. Note that even if the graph
 * has no self-loops, we may obtain a self-loop in the tree because its nodes
 * represent (potentially non-trivial) strongly connected sets.
 *
 * The edges of the tree are thought of as going from the root to the leaves
 * (for this description we ignore that in our implementation they are directed
 * the other way around). Backward and forward edges connect nodes on the same
 * tree branch: the backward ones go up the tree and the forward ones down the
 * tree. Cross edges connect nodes in different branches. Cross (non-)forward
 * edges go from a node to another node that has a (smaller) bigger height.
 *
 * Forward edges and self-loops are ignored. Backward edges mean that strongly
 * connected sets of the graph represented by the nodes of the tree that are
 * between the edge ends all belong to the same SCC. For such edges, the
 * corresponding sets in UnionFind are merged and the (sub-)path of the tree
 * between the edge ends is collapsed to one node. Cross forward edges are just
 * added to the next steam, no tree update happens. Cross non-forward edges
 * (a,b) constitute the most involved case. Let us say, b's parent is p. (Then
 * p is not a because (a, b) is a cross edge.) Then the tree edge (p, b) is
 * replaced by the edge (a, b) and (p, b) is added to the next stream.
 *
 * Complexity.
 *
 * The authors of the paper claim that the time complexity of the algorithm is
 * O(h*m + (n*log n)) where n is the number of vertices, m the number of edges
 * and h the maximal height of the tree that is reached during the algorithm
 * run. Theoretically, the best known upper bound for h is n, however, the
 * authors claim that in their experiments it is close to log n.
 *
 * Discussion.
 *
 * For the given practical upper bound, the asymptotic running time is by a
 * factor of log n worse than that of a classical algorithm as, e.g., Tarjan's
 * algorithm. The advantages are that
 * (1) we can start computing while the edges
 * are still being read and
 * (2) potentially we can parallelize the algorithm,
 * see http://snap.stanford.edu/class/cs224w-2017/projects/cs224w-9-final.pdf.
 *
 * Implementation details.
 *
 * The constructor only reads the vertices of the graph because reading the
 * edges is tightly associated with the first stream. The remainder is performed
 * in the function readEdgesBuildSCCs. The first stream is read directly from
 * the input in the member function readEdges from the parent class. It gets the
 * function onReadEdge, which is passed as a parameter and is executed on
 * each edge. The output is written into the next stream that is a queue.
 *
 * Back to readEdgesBuildSCCs, other streams are read from the input queue and
 * the output is written into the output queue. The roles of the queues are
 * switched between the iterations.
 *
 * The most involved function edgeType is described in the comments to the
 * function.
 *
 * @tparam EdgeProperties
 * @tparam VertexProperties
 */
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
  auto processEdge(VertexIndex from, VertexIndex to,
                   std::queue<std::pair<VertexIndex, VertexIndex>>& outputQueue)
      -> void;
  auto getTreeParent(VertexIndex idx) -> VertexIndex& {
    return this->vertices[idx].properties.treeParent;
  };
  auto edgeType(VertexIndex from, VertexIndex to) -> EdgeType;
  auto collapse(VertexIndex from, VertexIndex to) -> void;
};
