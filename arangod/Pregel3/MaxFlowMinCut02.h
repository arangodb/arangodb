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
#include "Graph02.h"
#include "Basics/ResultT.h"
#include "Containers/FlatHashSet.h"
#include "Containers/FlatHashMap.h"
#include "Algorithm.h"

namespace arangodb::pregel3 {

// indexes of edges with positive flow
typedef containers::FlatHashMap<size_t, double> Flow;

struct Cut {
  // indexes of edges in the cut
  containers::FlatHashSet<size_t> edges;
  // indexes of vertices inducing the the component of the graph without edges
  // of cut that contains _source (component(_g - cut) containing _source)
  containers::FlatHashSet<size_t> sourceComp;
};

struct MaxFlowMinCutResult : AlgorithmResult {
  MaxFlowMinCutResult() = default;  // if _target not reachable from _source
  MaxFlowMinCutResult(Flow&& f, Cut&& c, MinCutGraph* g)
      : flow(f), cut(c), _g(g) {}
  Flow flow;
  Cut cut;

 private:
  MinCutGraph* _g;

  void toVelocyPack(VPackBuilder& builder) override;
};

class MaxFlowMinCut : public Algorithm {
 public:
  MaxFlowMinCut(MinCutGraph* g, size_t source, size_t target)
      : _source(source), _target(target), _g(g){};
  ~MaxFlowMinCut() override = default;

 private:
  /**
   * Check that _source and _target are, indeed, indexes of existing vertices
   * @return
   */
  Result verifyInput() {
    if (_source >= _g->numVertices())
      return {Result(TRI_ERROR_BAD_PARAMETER,
                     "Wrong sourceId: " + std::to_string(_source) +
                         ", but the graph has only " +
                         std::to_string(_g->numVertices()) + " vertices.")};
    if (_target >= _g->numVertices()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Wrong targetId: " + std::to_string(_target) +
                  ", but the graph has only " +
                  std::to_string(_g->numVertices()) + " vertices."};
    }
  }

  std::unique_ptr<AlgorithmResult> run() override;
  void push(size_t a, size_t b);

  double excess(size_t u) const {
    TRI_ASSERT(u < _g->numVertices());
    return _g->vertices[u].excess;
  }

  size_t label(size_t u) const {
    TRI_ASSERT(u < _g->numVertices());
    return _g->vertices[u].label;
  }

  void setLabel(size_t u, size_t val) {
    TRI_ASSERT(u < _g->numVertices());
    _g->vertices[u].label = val;
  }

  //  double getCapacity(size_t u, size_t idxNeighbor, size_t idxEdge) const {
  //    TRI_ASSERT(u < _g->numVertices());
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.size() > idxNeighbor);
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.at(idxNeighbor).size() >
  //               idxEdge);
  //
  //    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
  //    return _g->edges.at(edgeIdx).capacity;
  //  }

  //  double getCapacity(size_t u, size_t idxNeighbor) const {
  //    TRI_ASSERT(u < _g->numVertices());
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.size() > idxNeighbor);
  //
  //    size_t const edgeIdx = getEdge(u, idxNeighbor);
  //    return _g->edges.at(edgeIdx).capacity;
  //  }

  double getCapacity(size_t e) const {
    TRI_ASSERT(e < _g->numEdges());
    return edge(e)->capacity;
  }

  //  void setCapacity(size_t u, size_t idxNeighbor, size_t idxEdge, double val)
  //  {
  //    TRI_ASSERT(u < _g->numVertices());
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.size() > idxNeighbor);
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.at(idxNeighbor).size() >
  //               idxEdge);
  //
  //    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
  //    _g->edges[edgeIdx].capacity = val;
  //  }

  //  void setCapacity(size_t u, size_t idxNeighbor, double val) const {
  //    TRI_ASSERT(u < _g->numVertices());
  //    TRI_ASSERT(_g->vertices.at(u).outEdges.size() > idxNeighbor);
  //
  //    size_t const edgeIdx = getEdge(u, idxNeighbor);
  //    _g->edges.at(edgeIdx).capacity = val;
  //  }

  //  void setCapacity(size_t e, double val) const {
  //    TRI_ASSERT(e < _g->numEdges());
  //    _g->edges.at(e).capacity = val;
  //  }

  /**
   * Return the flow of the edge from u to a neighbor v with index
   * idxNeighbor in the list of neighbors where the index of the edge is idxEdge
   * in the list of edges between u and v. Assumes that the idxNeighbor-th
   * neighbor exists and there are at least idxEdges many edges from u to v.
   * @param u
   * @param idxNeighbor
   * @param idxEdge
   * @return the stored flow
   */

  //  double flow(size_t u, size_t idxNeighbor, size_t idxEdge) const {
  //    TRI_ASSERT(u < _g->numVertices());
  //    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);
  //    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.at(idxNeighbor).size() >
  //               idxEdge);
  //
  //    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
  //    return _g->edges.at(edgeIdx).flow;
  //  }

  double flow(size_t eIdx) {
    TRI_ASSERT(_g->edges.contains(eIdx));
    return edge(eIdx)->flow;
  }

  double residual(size_t eIdx) {
    TRI_ASSERT(eIdx < _g->numEdges());
    auto const& e = edge(eIdx);
    return residual(e);
  }

  static double residual(MinCutEdge* e) {
    TRI_ASSERT(e->capacity >= e->flow);
    return e->capacity - e->flow;
  }

  static double residual(MinCutEdge const& e) {
    TRI_ASSERT(e.capacity >= e.flow);
    return e.capacity - e.flow;
  }

  double residual(size_t uIdx, size_t vIdx) const {
    return residual(edge(uIdx, vIdx));
  }

  void increaseFlow(size_t eIdx, double val) {
    TRI_ASSERT(eIdx < _g->numEdges());
    increaseFlow(edge(eIdx), val);
  }

  void increaseFlow(MinCutEdge* e, double val) { e->increaseFlow(val); }

  void decreaseFlow(size_t eIdx, double val) {
    TRI_ASSERT(eIdx < _g->numEdges());
    auto e = edge(eIdx);
    e->flow -= val;
    TRI_ASSERT(e->flow >= 0);
  }

  //  void increaseFlowNeighb(size_t u, size_t idxNeighbor, double val) {
  //    TRI_ASSERT(val > 0);
  //    _g->edgeProperties[getEdge(u, idxNeighbor)].flow += val;
  //  }
  //
  //  void decreaseFlowNeighb(size_t u, size_t idxNeighbor, double val) {
  //    TRI_ASSERT(val > 0);
  //    _g->edgeProperties[getEdge(u, idxNeighbor)].flow -= val;
  //  }

  void setFlow(size_t eIdx, double val) {
    TRI_ASSERT(eIdx < _g->numEdges());
    TRI_ASSERT(val >= 0);
    edge(eIdx)->flow = val;
  }

  void increaseExcess(size_t u, double val) {
    TRI_ASSERT(val > 0);
    TRI_ASSERT(u < _g->numVertices());
    _g->vertices[u].excess += val;
  }

  void decreaseExcess(size_t u, double val) {
    TRI_ASSERT(val > 0);
    TRI_ASSERT(u < _g->numVertices());
    _g->vertices[u].excess += val;
    TRI_ASSERT(_g->vertices[u].excess >= 0);
  }

  size_t outDegree(size_t u) const {
    TRI_ASSERT(u < _g->numVertices());
    return _g->vertices[u].outDegree();
  }

  MinCutVertex& vertex(size_t vIdx) { return _g->vertex(vIdx); }
  MinCutVertex& vertex(size_t vIdx) const { return _g->vertex(vIdx); }

  MinCutEdge* edge(size_t eIdx) { return _g->edge(eIdx); }
  MinCutEdge* edge(size_t eIdx) const { return _g->edge(eIdx); }
  MinCutEdge* edge(size_t uIdx, size_t vIdx) const {
    return _g->edge(uIdx, vIdx);
  }

 private:
  size_t _source;
  size_t _target;
  // e = (u,v) is applicable if excess(u) > 0, label(u) = label(v) + 1 and
  // residual(e) > 0
  containers::FlatHashSet<std::pair<size_t, size_t>> _applicableEdges;

  // A vertex u is relabable if excess(u) > 0 and for all out-neighbors of u
  // with index idxNeighb, if residual(u, idxNeighb) > 0, then label(u) <=
  // label(v)
  std::unordered_set<size_t> _relabableVertices;
  MinCutGraph* _g;
  void initialize();
  void relabel(size_t uIdx);
  // unknown getNeighborByIdx(size_t u, size_t idxNeighb);
  void updateRelabableAfterRelabel(MinCutVertex const& u, size_t uIdx,
                                   size_t oldLabel);
  void removeLeavesRecursively();
  bool existsEdge(MinCutEdge* eIdx);
  void updateApplicableAfterPush(const MinCutVertex& v, const MinCutVertex& u,
                                 double delta);
  void updateRelabableAfterPush(const MinCutVertex& v, size_t vIdx,
                                const MinCutVertex& u, size_t uIdx);
  void updateApplicableAfterRelabel(const MinCutVertex& u, size_t uIdx);
};

}  // namespace arangodb::pregel3