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

#include "MaxFlowMinCut02.h"
#include <vector>

namespace arangodb::pregel3 {

void MaxFlowMinCut::removeLeavesRecursively() {
  // make _target non-leaf
  bool tmpVertex = false;
  if (vertex(_target).outDegree() == 0) {
    tmpVertex = true;
    size_t tmpVertexIdx = _g->numVertices();
    _g->addVertex();
    _g->addEdge(_target, tmpVertexIdx);
    _g->addEdge(tmpVertexIdx, _target);
  }

  std::vector<size_t> currentLeaves;
  std::vector<size_t> numSucc(_g->numVertices());
  // compute initial leaves and number of successors for each vertex
  for (size_t vIdx = 0; vIdx < _g->vertices.size(); ++vIdx) {
    numSucc[vIdx] = _g->vertices[vIdx].outDegree();
    if (numSucc[vIdx] == 0) {
      currentLeaves.push_back(vIdx);
    }
  }

  // initialize allLeaves
  std::unordered_set<size_t> allLeaves;  // todo make it a bitset
  for (size_t i : currentLeaves) {
    allLeaves.insert(i);
  }

  // recursively mark leaves as such (push them to allLeaves), update numSucc
  while (!currentLeaves.empty()) {
    size_t vIdx = currentLeaves.back();
    currentLeaves.pop_back();
    MinCutVertex const& v = vertex(vIdx);
    for (auto const& [uIdx, e] : v.inEdges) {
      numSucc[uIdx]--;
      if (numSucc[uIdx] == 0) {
        currentLeaves.push_back(uIdx);
        allLeaves.insert(uIdx);
      }
    }
  }
  // remove marked leaves, compress _g->vertices
  // iterate over _g->vertices with two indexes: i, j. Index i points to the
  // first position with a leaf, index j to the first position after a leaf
  // with a non-leaf. Then _g->vertices[j] is copied to _g[vertices[i] and
  // the indexes are updated.
  size_t i = 0;
  size_t const n = _g->vertices.size();
  while (true) {
    // set i
    while (i < n && !allLeaves.contains(i)) {
      ++i;
    }
    if (i == n) {
      // no leaves more, done
      _g->vertices.resize(i);
      return;
    }
    // set j
    size_t j = i + 1;
    while (j < n && allLeaves.contains(j)) {
      ++j;
    }
    if (j == n) {
      // only non-leaves after leaves, no "holes" more
      _g->vertices.resize(i);
      return;
    }
    // copy
    _g->vertices[i] = _g->vertices[j];
    auto v = vertex(i);
    // update edges, inEdges and outEdges
    for (auto& [toIdx, e] : v.outEdges) {
      auto& to = vertex(toIdx);
      auto handle = to.inEdges.extract(j);
      handle.key() = i;
      to.inEdges.insert(std::move(handle));
      e->from = i;
    }
    for (auto& [fromIdx, e] : v.inEdges) {
      auto& from = vertex(fromIdx);
      auto handle = from.outEdges.extract(j);
      handle.key() = i;
      from.inEdges.insert(std::move(handle));
      e->from = i;
    }
    // increase i (and j): this guarantees that i finally reaches n
    // and the outer while loop terminates
    i++;
    j++;
  }

  if (tmpVertex) {
    _g->removeEdge(edge(_target, _g->numVertices() - 1));
    _g->removeEdge(edge(_g->numVertices() - 1, _target));
    _g->vertices.pop_back();
  }
}
void MaxFlowMinCut::initialize() {
  removeLeavesRecursively();

  setLabel(_source, _g->numVertices());

  // for all pairs of vertices (u,v) with multiple edges create an additional
  // edge and shift all capacities to this new edge
  // such that the last edge always has the sum of all capacities

  // currently not implemented as we don't have multiple edges
  /*
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
  */
}

void MaxFlowMinCut::push(size_t uIdx, size_t vIdx) {
  TRI_ASSERT(uIdx < _g->numEdges());
  TRI_ASSERT(vIdx < _g->numEdges());
  auto e = edge(uIdx, vIdx);
  TRI_ASSERT(e->from < _g->numVertices());
  TRI_ASSERT(e->to < _g->numVertices());
  auto& u = _g->vertices[uIdx];
  auto& v = _g->vertices[vIdx];
  TRI_ASSERT(u.excess > 0);
  TRI_ASSERT(u.label == v.label + 1);

  double delta = std::min(u.excess, residual(e));
  e->increaseFlow(delta);

  auto eRev = _g->reverseEdge(e);
  eRev->decreaseFlow(delta);
  u.decreaseExcess(delta);

  // UPDATE _applicableEdges

  // check if the edge is not applicable any more. labels have not been changed,
  // so they must not be checked.
  // other applicable edges remained applicable
  if (u.excess <= 0 || residual(e) <= 0) {
    _applicableEdges.erase(std::make_pair(e->from, e->to));
  }
  // check if, for out-neighbors nv of v, the edge (v, nv) becomes applicable
  // (including nv == u)
  if (v.excess - delta == 0) {  // otherwise v has already had positive excess
                                // and nothing changed for edges (v, nv)
    // todo introduce data structure keeping track of outgoing edges with
    // positive  residual capacity (and, possibly, not smaller label).
    // for now, just iterate
    for (auto const& [neighbIdx, outE] : u.outEdges) {
      auto const neighb = vertex(neighbIdx);
      if (outE->residual() > 0 && v.label == neighb.label + 1) {
        _applicableEdges.insert(std::make_pair(outE->from, outE->to));
      }
    }
  }
  // for edges (nu, u) with nu != v nothing changed

  // each applicable edge (u, nu) with nu != v becomes non-applicable
  // if excess(u) became 0
  if (u.excess == 0) {
    for (auto const& [neighbIdx, outE] : u.outEdges) {
      _applicableEdges.erase(std::make_pair(outE->from, neighbIdx));
    }
  }

  // END UPDATE _applicableEdges

  // UPDATE _relabableVertices

  // check if u still has excess
  if (u.excess == 0) {
    _relabableVertices.erase(uIdx);
  }

  // check if v became relabable (note: delta > 0 and thus excess(v) > 0)
  // todo introduce data structure keeping track of outgoing edges with positive
  //  residual capacity (and, possibly, not smaller label).
  // for now, just iterate
  bool isRelabable = true;
  for (auto const& [neighbV, outE] : v.outEdges) {
    if (outE->residual() > 0 && v.label > vertex(neighbV).label) {
      isRelabable = false;
      break;
    }
  }
  if (isRelabable) {
    _relabableVertices.insert(vIdx);
  }
  // for other vertices, whether they can be relabeled didn't change
  // END UPDATE _relabableVertices
}

// update _relabableVertices: the only possible change is that
// in-neighbors v of u can become relabable: if u was the only out-neighbor of
// v with smaller label and residual(v,u) > 0 and now (after relabelling)
// label(v) <= label(u).
// Note: to get all in-neighbors in the residual graph, we have to iterate
// over out-neighbors v as well and check whether flow(u,v) > 0 (then (v, u)
// exists in the residual graph).
// Note: Maybe more efficient: store how many out-neighbors
// y a vertex x has with capacity(x,y) > 0 and label(x) > label(y).
void MaxFlowMinCut::updateRelabable(MinCutVertex const& u, size_t oldLabel) {
  for (auto const& [vIdx, vu] : u.inEdges) {
    if (_relabableVertices.find(vIdx) != _relabableVertices.end()) {
      continue;  // already relabable
    }
    auto const& v = _g->vertices[vIdx];

    if (v.excess > 0 && residual(vu) > 0 && v.label > oldLabel &&
        v.label <= u.label) {  // filter
      bool isRelabable = true;
      // iterate over all out-neighbors of v (including u)
      for (auto const& [wIdx, vw] : v.outEdges) {
        auto const& w = vertex(wIdx);
        if (residual(vw) > 0 && w.label < v.label) {
          isRelabable = false;
          break;
        }
      }
      if (isRelabable) {
        _relabableVertices.insert(vIdx);
        return;
      }
    }
  }
}

void MaxFlowMinCut::relabel(size_t uIdx) {
  TRI_ASSERT(uIdx < _g->numVertices());
  auto& u = _g->vertices[uIdx];
  TRI_ASSERT(u.excess > 0);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // assert that the label of u is at most the label of its out-neighbors
  // for all neighbors v with positive residual capacity from u to v
  for (auto const& [neighb, outE] : u.outEdges) {
    TRI_ASSERT(residual(outE) <= 0 || u.label <= vertex(neighb).label);
  }
#endif
  size_t minLabelNeighb = _g->numVertices();
  for (auto const& [v, e] : u.outEdges) {
    if (residual(e) > 0) {
      minLabelNeighb = std::min(minLabelNeighb, vertex(v).label);
    }
  }
  for (auto const& [vIdx, e] :
       u.inEdges) {  // real edge: vu; in residual graph: uv
    if (e->flow > 0) {
      minLabelNeighb = std::min(minLabelNeighb, vertex(vIdx).label);
    }
  }
  size_t const oldLabel = u.label;
  u.label = minLabelNeighb + 1;
  _relabableVertices.erase(uIdx);

  // note: applicability did not change

  // update _relabableVertices: the only possible change is that
  // in-neighbors v of u can become relabable because label(u) increased
  updateRelabable(u, oldLabel);
}

MaxFlowMinCutResult MaxFlowMinCut::run() {
  initialize();

  // saturate all out-edges of _source
  // todo: iterator over outgoing edges
  for (auto const& [vIdx, e] : _g->vertices[_source].outEdges) {
    e->flow = e->capacity;
    _relabableVertices.insert(vIdx);
  }
  // Note: graph _g and residual graph are still the same, no need to iterate
  // over _g->vertices[_source].inEdges

  while (!_applicableEdges.empty() || !_relabableVertices.empty()) {
    // try to push prior to relabelling
    if (!_applicableEdges.empty()) {
      auto const& [u, idxNeighb] = *_applicableEdges.begin();
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
    for (auto const& [vIdx, e] : vertex(u).outEdges) {  // todo also inEdges
      if (e->residual() == 0) {
        continue;
      }
      if (!c.sourceComp.contains(vIdx)) {
        queue.push_back(vIdx);
        c.sourceComp.insert(vIdx);
      }
    }
  }

  // for each vertex in c.sourceComp, add every outgoing edge leading to an
  // other component to c.edges
  for (size_t uIdx : c.sourceComp) {
    auto const& u = vertex(uIdx);
    for (auto const& [vIdx, e] : u.outEdges) {
      if (e->residual() <= 0) {
        continue;
      }
      if (!c.sourceComp.contains(vIdx)) {
        c.edges.insert(e->idx);
      }
    }
  }

  Flow f;
  // share the flow from the last edge in a multiedge among all edges
  // and populate f
  for (size_t uIdx = 0; uIdx < _g->numVertices(); ++uIdx) {
    auto const& u = vertex(uIdx);
    for (auto const& [_, e] : u.outEdges) {  // todo
      if (e->flow > 0) {
        f.insert(e->idx);
      }
    }
  }
  return {std::move(f), std::move(c)};
}

}  // namespace arangodb::pregel3