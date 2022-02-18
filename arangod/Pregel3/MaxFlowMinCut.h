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
#include "Graph.h"
#include "Basics/ResultT.h"
#include "Containers/FlatHashSet.h"

namespace arangodb::pregel3 {

// edge index to flow
typedef containers::FlatHashMap<size_t, double> Flow;

struct Cut {
  // indexes of edges in the cut
  containers::FlatHashSet<size_t> edges;
  // indexes of vertices inducing the the component of the graph without edges
  // of cut that contains _source (component(_g - cut) containing _source)
  containers::FlatHashSet<size_t> sourceComp;
};

struct MaxFlowMinCutResult {
  MaxFlowMinCutResult() = default;  // if _target not reachable from _source
  MaxFlowMinCutResult(Flow&& f, Cut&& c) : flow(f), cut(c) {}
  Flow flow;
  Cut cut;
};

class MaxFlowMinCut {
  MaxFlowMinCut(MinCutGraph* g, size_t source, size_t target)
      : _source(source), _target(target), _g(g) {}

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
  /**
   * If the input is correct, compute a maximum flow and the corresponding
   * mincut. Otherwise return TRI_ERROR_BAD_PARAMETER.
   * @param g
   * @param sourceIdx
   * @param targetIdx
   * @return
   */

  MaxFlowMinCutResult run();
  void push(size_t u, size_t idxNeighb);

  double excess(size_t u) const { return _g->vertexProperties.at(u).excess; }

  size_t label(size_t u) const { return _g->vertexProperties.at(u).label; }

  void setLabel(size_t u, size_t val) {
    _g->vertexProperties.at(u).label = val;
  }

  /**
   * Return the capacity of the edge from u to a neighbor v with index
   * idxNeighbor in the list of neighbors where the index of the edge is idxEdge
   * in the list of edges between u and v. Assumes that the idxNeighbor-th
   * neighbor exists and there are at least idxEdges many edges from u to v.
   * @param u
   * @param idxNeighbor
   * @param idxEdge
   * @return the stored capacity
   */
  double getCapacity(size_t u, size_t idxNeighbor, size_t idxEdge) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.at(idxNeighbor).size() >
               idxEdge);

    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
    return _g->edgeProperties.at(edgeIdx).capacity;
  }

  double getCapacity(size_t u, size_t idxNeighbor) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);

    size_t const edgeIdx = getEdge(u, idxNeighbor);
    return _g->edgeProperties.at(edgeIdx).capacity;
  }

  double getCapacity(size_t e) const {
    TRI_ASSERT(e < _g->numEdges());
    return _g->edgeProperties.at(e).capacity;
  }

  void setCapacity(size_t u, size_t idxNeighbor, size_t idxEdge, double val) {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.at(idxNeighbor).size() >
               idxEdge);

    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
    _g->edgeProperties.at(edgeIdx).capacity = val;
  }

  void setCapacity(size_t u, size_t idxNeighbor, double val) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);

    size_t const edgeIdx = getEdge(u, idxNeighbor);
    _g->edgeProperties.at(edgeIdx).capacity = val;
  }

  void setCapacity(size_t e, double val) const {
    TRI_ASSERT(e < _g->numEdges());
    _g->edgeProperties.at(e).capacity = val;
  }

  /**
   * Return the preflow of the edge from u to a neighbor v with index
   * idxNeighbor in the list of neighbors where the index of the edge is idxEdge
   * in the list of edges between u and v. Assumes that the idxNeighbor-th
   * neighbor exists and there are at least idxEdges many edges from u to v.
   * @param u
   * @param idxNeighbor
   * @param idxEdge
   * @return the stored preflow
   */

  double preflow(size_t u, size_t idxNeighbor, size_t idxEdge) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.at(idxNeighbor).size() >
               idxEdge);

    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
    return _g->edgeProperties.at(edgeIdx).preflow;
  }

  size_t getEdge(size_t u, size_t idxNeighbor, size_t idxEdge) const {
    return _g->vertexProperties.at(u).outEdges.at(idxNeighbor).at(idxEdge);
  }

  size_t getEdge(size_t u, size_t idxNeighbor) const {
    return _g->vertexProperties.at(u).outEdges.at(idxNeighbor).back();
  }

  double getFlowNeighb(size_t u, size_t idxNeighb) const {
    return _g->edgeProperties.at(getEdge(u, idxNeighb)).preflow;
  }

  double residualNeighb(size_t u, size_t idxNeighbor, size_t idxEdge) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.at(idxNeighbor).size() >
               idxEdge);

    size_t const edgeIdx = getEdge(u, idxNeighbor, idxEdge);
    return _g->edgeProperties.at(edgeIdx).capacity -
           _g->edgeProperties.at(edgeIdx).preflow;
  }

  size_t neighbToIdx(size_t u, size_t idxNeighb) const {
    return _g->vertexProperties.at(u).neighborsReverse.at(idxNeighb);
  }

  double residualNeighb(size_t u, size_t idxNeighbor) const {
    TRI_ASSERT(u < _g->numVertices());
    TRI_ASSERT(_g->vertexProperties.at(u).outEdges.size() > idxNeighbor);

    size_t const edgeIdx = getEdge(u, idxNeighbor);
    return _g->edgeProperties.at(edgeIdx).capacity -
           _g->edgeProperties.at(edgeIdx).preflow;
  }

  double residual(size_t u, size_t v) {
    size_t const idxNeighb = _g->vertexProperties.at(u).neighborsReverse.at(v);
    return residualNeighb(u, idxNeighb);
  }

  void increaseFlowNeighb(size_t u, size_t idxNeighbor, double val) {
    TRI_ASSERT(val > 0);
    _g->edgeProperties[getEdge(u, idxNeighbor)].preflow += val;
  }

  void decreaseFlowNeighb(size_t u, size_t idxNeighbor, double val) {
    TRI_ASSERT(val > 0);
    _g->edgeProperties[getEdge(u, idxNeighbor)].preflow -= val;
  }

  void setFlow(size_t e, double val) { _g->edgeProperties[e].preflow = val; }

  void increaseExcess(size_t u, double val) {
    TRI_ASSERT(val > 0);
    _g->vertexProperties[u].excess += val;
  }

  void decreaseExcess(size_t u, double val) {
    TRI_ASSERT(val > 0);
    _g->vertexProperties[u].excess -= val;
  }

  bool isLeaf(size_t v) const { return _g->vertexProperties.at(v).leaf; }
  // todo: iterator over non-leaf neighbors

  void setLeaf(size_t v) { _g->vertexProperties[v].leaf = true; }

  void unsetLeaf(size_t v) { _g->vertexProperties[v].leaf = false; }

  size_t outDegree(size_t v) const { return _g->outDegree(v); }

 private:
  size_t _source;
  size_t _target;
  // e = (u,v) is applicable if excess(u) > 0, label(u) = label(v) + 1 and
  // residual(e) > 0
  // an entry
  containers::FlatHashSet<std::pair<size_t, size_t>> _applicableEdges;

  // A vertex u is relabable if excess(u) > 0 and for all out-neighbors of u
  // with index idxNeighb, if residual(u, idxNeighb) > 0, then label(u) <=
  // label(v)
  std::unordered_set<size_t> _relabableVertices;
  MinCutGraph* _g;
  void initialize();
  void relabel(size_t u);
};

}  // namespace arangodb::pregel3