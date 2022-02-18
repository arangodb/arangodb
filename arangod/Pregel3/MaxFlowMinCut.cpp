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

#include "MaxFlowMinCut.h"
#include "Containers/FlatHashSet.h"

namespace arangodb::pregel3 {

void MaxFlowMinCut::initialize() {
  // set leaves
  for (size_t v = 0; v < _g->numVertices(); ++v) {
    if (outDegree(v) == 0) {
      setLeaf(v);
    }
  }
  unsetLeaf(_target);

  setLabel(_source, _g->numVertices());

  // for all pairs of vertices (u,v) with multiple edges create an additional
  // edge and shift all capacities to this new edge
  // such that the last edge always has the sum of all capacities
  for (size_t v = 0; v < _g->numVertices(); ++v) {
    if (isLeaf(v)) {
      continue;
    }
    for (size_t idxNeighb = 0; idxNeighb < _g->outDegree(v); ++v) {
      if (_g->vertexProperties.at(v).outEdges.at(idxNeighb).size() > 1) {
        double sumCapacities = 0.0;
        for (auto const& e : _g->getEdgesNeighb(v, idxNeighb)) {
          sumCapacities += _g->edgeProperties.at(e).capacity;
        }
        auto e = _g->addEdge(v, idxNeighb);
        setCapacity(e, sumCapacities);
      }
    }
  }
}

void MaxFlowMinCut::push(size_t u, size_t idxNeighb) {
  TRI_ASSERT(!isLeaf(u));
  // the other vertex
  size_t const v = _g->vertexProperties.at(u).neighbors.at(idxNeighb);
  TRI_ASSERT(!isLeaf(v));
  TRI_ASSERT(excess(u) > 0);
  TRI_ASSERT(label(u) == label(v) + 1);

  double delta = std::min(excess(u), residualNeighb(u, idxNeighb));
  increaseFlowNeighb(u, idxNeighb, delta);
  // u is the idxU-th neighbor of v
  auto [_, idxU] = _g->invertEdge(u, idxNeighb);
  decreaseFlowNeighb(v, idxU, delta);
  decreaseExcess(u, delta);
  increaseExcess(v, delta);

  // UPDATE _applicableEdges

  // check if the edge is not applicable any more. labels have not been changed,
  // so they must not be checked.
  // other applicable edges remained applicable
  if (excess(u) <= 0 || residualNeighb(u, idxNeighb) <= 0) {
    _applicableEdges.erase(std::make_pair(u, idxNeighb));
  }
  // check if, for out-neighbors nv of v, the edge (v, nv) becomes applicable
  // (including nv == u)
  if (excess(v) - delta == 0) {  // otherwise v has already had positive excess
                                 // and nothing changed for edges (v, nv)
    // todo introduce data structure keeping track of outgoing edges with
    // positive
    //  residual capacity (and, possibly, not smaller label).
    // for now, just iterate
    for (size_t i = 0; i < outDegree(_source); ++i) {
      if (isLeaf(neighbToIdx(_source, i))) {
        continue;
      }
      size_t nv = _g->vertexProperties.at(v).neighbors.at(i);
      if (residualNeighb(u, i) > 0 && label(v) == label(nv) + 1) {
        _applicableEdges.insert(std::make_pair(v, nv));
      }
    }
  }
  // for edges (nu, u) with nu != v nothing changed

  // each applicable edge (u, nu) with nu != v became non-applicable
  // if excess(u) became 0
  if (excess(u) == 0) {
    for (size_t i = 0; i < outDegree(_source); ++i) {
      _applicableEdges.erase(
          std::make_pair(u, _g->vertexProperties.at(u).neighbors.at(i)));
    }
  }

  // END UPDATE _applicableEdges

  // UPDATE _relabableVertices

  // check if u still has excess
  if (excess(u) == 0) {
    _relabableVertices.erase(u);
  }

  // check if v became relabable (note: delta > 0 and thus excess(v) > 0)
  // todo introduce data structure keeping track of outgoing edges with positive
  //  residual capacity (and, possibly, not smaller label).
  // for now, just iterate
  bool isRelabable = true;
  for (size_t i = 0; i < outDegree(v); ++i) {
    size_t const nv = neighbToIdx(v, i);
    if (isLeaf(nv)) {
      continue;
    }
    if (residualNeighb(v, i) > 0 && label(v) > label(nv)) {
      isRelabable = false;
      break;
    }
  }
  if (isRelabable) {
    _relabableVertices.insert(v);
  }
  // for other vertices, whether they can be relabeled didn't change
  // END UPDATE _relabableVertices
}

void MaxFlowMinCut::relabel(size_t u) {
  TRI_ASSERT(excess(u) > 0);
  TRI_ASSERT(!isLeaf(u));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // assert that the label of u is at most the label of its out-neighbors
  // for all neighbors v with positive residual capacity from u to v
  for (size_t i = 0; i < outDegree(u); ++i) {
    if (!isLeaf(neighbToIdx(u, i))) {
      TRI_ASSERT(residualNeighb(u, i) <= 0 ||
                 label(u) <= label(_g->vertexProperties[u].neighbors[i]));
    }
  }
#endif
  size_t const firstNeighb =
      _g->vertexProperties[u].neighbors[0];  // exists as u not a leaf
  size_t minLabelNeighb = label(firstNeighb);
  for (size_t i = 1; i < outDegree(u); ++i) {
    if (residualNeighb(u, i) > 0) {
      minLabelNeighb = std::min(
          minLabelNeighb, label(_g->vertexProperties.at(u).neighbors.at(i)));
    }
  }
  setLabel(u, minLabelNeighb);
  _relabableVertices.erase(u);

  // note: applicability did not change

  // update _relabableVertices: the only possible change is that
  // in-neighbors can become relabable. we can get them because the graph is
  // undirected
  for (size_t v : _g->vertexProperties.at(u).neighbors) {
    if (_relabableVertices.find(v) != _relabableVertices.end()) {
      continue;  // already relabable
    }
    size_t idxU = neighbToIdx(v, u);
    if (excess(v) > 0 &&
        residualNeighb(v, idxU) /*the edge exists in the residual graph*/ > 0) {
      bool isRelabable = true;
      for (size_t i = 0; i < outDegree(v); ++i) {
        size_t const w = neighbToIdx(v, i);
        if (residualNeighb(v, i) > 0 && label(v) > label(w)) {
          isRelabable = false;
          break;
        }
      }
      if (isRelabable) {
        _relabableVertices.insert(v);
      }
    }
  }
}

MaxFlowMinCutResult MaxFlowMinCut::run() {
  if (_g->graphProperties.isDirected) {
    _g->makeUndirected();
  }

  initialize();

  if (isLeaf(_source)) {
    return {};
  }

  // saturate all out-edges of _source
  // todo: iterator over outgoing edges
  for (size_t i = 0; i < outDegree(_source); ++i) {
    if (isLeaf(neighbToIdx(_source, i))) {
      continue;
    }
    size_t const e = _g->vertexProperties.at(_source).outEdges.at(i).back();
    setFlow(e, getCapacity(e));
    _relabableVertices.insert(neighbToIdx(_source, i));
  }

  while (!_applicableEdges.empty() || !_relabableVertices.empty()) {
    // try to push prior to relabelling
    if (!_applicableEdges.empty()) {
      auto const [u, idxNeighb] = *_applicableEdges.begin();
      push(u, idxNeighb);
    } else {
      if (!_relabableVertices.empty()) {
        auto const u = *_relabableVertices.begin();
        relabel(u);
      }
    }
  }

  Cut c;
  // with bfs search from _source in the residual graph,
  // add all found vertices to c.sourceComp
  std::deque<size_t> queue;
  queue.push_back(_source);
  while (!queue.empty()) {
    size_t const u = queue.front();
    queue.pop_front();
    for (size_t idxNeighb = 0; idxNeighb < outDegree(u); ++idxNeighb) {
      if (residualNeighb(u, idxNeighb) == 0) {
        continue;
      }
      size_t const v = neighbToIdx(u, idxNeighb);
      if (!c.sourceComp.contains(v)) {
        queue.push_back(v);
        c.sourceComp.insert(v);
      }
    }
  }

  // for each vertex in c.sourceComp, add every outgoing edge leading to an
  // other component to c.edges
  for (size_t u : c.sourceComp) {
    for (size_t idxNeighb = 0; idxNeighb < outDegree(u); ++idxNeighb) {
      if (residualNeighb(u, idxNeighb) <= 0) {
        continue;
      }
      if (!c.sourceComp.contains(neighbToIdx(u, idxNeighb))) {
        for (size_t e : _g->vertexProperties.at(u).outEdges.at(idxNeighb)) {
          c.edges.insert(e);
        }
        // remove the last artificial edge again
        if (_g->vertexProperties.at(u).outEdges.at(idxNeighb).size() > 1) {
          c.edges.erase(
              _g->vertexProperties.at(u).outEdges.at(idxNeighb).back());
        }
      }
    }
  }

  Flow f;
  // share the flow from the last edge in a multiedge among all edges
  // and populate f
  for (size_t u = 0; u < _g->numVertices(); ++u) {
    for (size_t idxNeighb = 0; idxNeighb < outDegree(u); ++idxNeighb) {
      if (getFlowNeighb(u, idxNeighb) > 0) {
        auto& edges = _g->vertexProperties.at(u).outEdges[idxNeighb];
        size_t const numEdges = edges.size();
        if (numEdges > 1) {  // otherwise the edge has the correct value
          size_t i = 0;
          while (i < numEdges && getFlowNeighb(u, idxNeighb) > 0) {
            double const delta =
                std::min(getCapacity(edges.at(i)), getFlowNeighb(u, idxNeighb));
            setFlow(edges[i], delta);
            size_t const lastEdge =
                _g->vertexProperties.at(u).outEdges[idxNeighb].back();
            _g->edgeProperties[lastEdge].preflow -= delta;
            f.insert(std::make_pair(edges[i], delta));
          }
        } else {
          f.insert(std::make_pair(edges.back(), getFlowNeighb(u, idxNeighb)));
        }
      }
    }
  }

  return {};
}

}  // namespace arangodb::pregel3