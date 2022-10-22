#include "SCCGraph.h"

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
SCCGraph<EdgeProperties, VertexProperties>::SCCGraph(
    const SharedSlice& graphJson, bool checkDuplicateVertices)
    : sccs(), _idxDummyRoot{graphJson["vertices"].length()} {
  this->readVertices(graphJson, checkDuplicateVertices);
  for (VertexIndex idx = 0; idx <= _idxDummyRoot; ++idx) {
    sccs.addSingleton(idx);
    this->getTreeParent(idx) = _idxDummyRoot;  // root is its own parent
  }
}

/**
 * Returns the type of the edge. We run from both edge ends in parallel towards
 * the root. The result depends on
 * (1) which end (call it X) reaches the least common predecessor (LCP, the
 * intersecting point of both paths) first, and
 * (2) if the other end (that reaches LCP later, call it Y) meets X itself or
 * another node on the path from X to the root.
 *
 * Recall that tree edges go from a representative of the source scc to, in
// general, any vertex in the target scc.
 * @tparam EdgeProperties
 * @tparam VertexProperties
 * @param from
 * @param to
 * @return
 */
template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto SCCGraph<EdgeProperties, VertexProperties>::edgeType(VertexIndex from,
                                                          VertexIndex to)
    -> EdgeType {
  from = sccs.representative(from);
  to = sccs.representative(to);
  if (from == to) {
    return EdgeType::FORWARD_OR_SELF_LOOP;  // self-loop
  }

  // run from nodes from and to in parallel the paths towards the root
  // until the paths intersect or until the root is reached
  auto runningFrom = from;
  auto runningTo = to;
  std::unordered_set<VertexIndex> traceOfFrom{runningFrom};
  std::unordered_set<VertexIndex> traceOfTo{runningTo};
  while (not traceOfTo.contains(runningFrom) and
         not traceOfFrom.contains(runningTo) and
         runningFrom != _idxDummyRoot and runningTo != _idxDummyRoot) {
    runningFrom = sccs.representative(this->getTreeParent(runningFrom));
    traceOfFrom.insert(runningFrom);
    runningTo = sccs.representative(this->getTreeParent(runningTo));
    traceOfTo.insert(runningTo);
  }

  // analyse why we left the while loop
  if (runningFrom == to) {
    // special case, in particular, traceOfTo.contains(runningFrom)
    return EdgeType::BACKWARD;
  }

  if (runningTo == from) {
    // special case, in particular, traceOfFrom.contains(runningTo)
    return EdgeType::FORWARD_OR_SELF_LOOP;  // forward
  }

  if (traceOfTo.contains(runningFrom)) {
    return EdgeType::CROSS_NON_FORWARD;
  }

  if (traceOfFrom.contains(runningTo)) {
    return EdgeType::CROSS_FORWARD;
  }

  // remaining cases: one of runningFrom and runningTo reached the root
  if (runningFrom == _idxDummyRoot) {
    while (not traceOfFrom.contains(runningTo)) {
      runningTo = sccs.representative(this->getTreeParent(runningTo));
    }
    if (runningTo == from) {
      return EdgeType::FORWARD_OR_SELF_LOOP;  // forward
    }
    return EdgeType::CROSS_FORWARD;
  } else {
    // runningTo == _idxDummyRoot
    while (not traceOfTo.contains(runningFrom)) {
      runningFrom = sccs.representative(this->getTreeParent(runningFrom));
    }
    if (runningFrom == to) {
      return EdgeType::BACKWARD;
    }
    return EdgeType::CROSS_NON_FORWARD;
  }
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto SCCGraph<EdgeProperties, VertexProperties>::collapse(VertexIndex from,
                                                          VertexIndex to)
    -> void {
  VertexIndex running = from;
  VertexIndex newRepresentative;
  while (running != to) {
    newRepresentative = sccs.merge(running, to);
    running = this->getTreeParent(running);
  }
  // note: we leave this->getTreeParent(running)
  // for running != newRepresentative as junk
  this->getTreeParent(newRepresentative) = this->getTreeParent(to);
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto SCCGraph<EdgeProperties, VertexProperties>::processEdge(
    VertexIndex from, VertexIndex to,
    std::queue<std::pair<VertexIndex, VertexIndex>>& outputQueue) -> void {
  auto representativeFrom = sccs.representative(from);
  auto representativeTo = sccs.representative(to);
  VertexIndex parentTo = this->getTreeParent(to);
  VertexIndex parentFrom = this->getTreeParent(from);

  /* the paper doesn't mention the second condition in the following "if",
   * but otherwise the tree structure can be destroyed if at the beginning
   * the edges (u,v) and then (v,u) appear. */
  if (parentTo == _idxDummyRoot and parentFrom == _idxDummyRoot) {
    this->getTreeParent(representativeTo) = from;
  } else {
    switch (edgeType(representativeFrom, representativeTo)) {
      case EdgeType::BACKWARD: {
        changedTree = true;
        collapse(representativeFrom, representativeTo);
        return;
      }
      case EdgeType::CROSS_FORWARD: {
        outputQueue.push(std::make_pair(representativeFrom, representativeTo));
        return;
      }
      case EdgeType::CROSS_NON_FORWARD: {
        changedTree = true;
        this->getTreeParent(representativeTo) = representativeFrom;
        outputQueue.push(
            std::make_pair(sccs.representative(parentTo), representativeTo));
        return;
      }
      default:
        return;  // forward edge or self-loop: do nothing, return no
                 // changes at the tree
    }
  }
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto SCCGraph<EdgeProperties, VertexProperties>::onReadEdge(SCCEdge const& edge)
    -> void {
  VertexIndex from = this->getVertexPosition(edge._from);
  VertexIndex to = this->getVertexPosition(edge._to);
  processEdge(from, to, this->_nextStream);
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto SCCGraph<EdgeProperties, VertexProperties>::readEdgesBuildSCCs(
    SharedSlice const& graphJson) -> void {
  // first stream: input from graphJson, output to _nextStream
  this->readEdges(graphJson, true, [&](SCCEdge edge) { onReadEdge(edge); });

  // remaining streams
  while (changedTree) {
    //      not _nextStream.empty() /*todo: test (or read paper) if this is
    //      correct*/ or changedTree) {
    changedTree = false;
    std::swap(_currentStream, _nextStream);

    while (not _currentStream.empty()) {
      std::pair<VertexIndex, VertexIndex> edge = _currentStream.front();
      _currentStream.pop();
      // this may set changedTree = true
      processEdge(edge.first, edge.second, _nextStream);
    }
  }
  sccs.removeElement(_idxDummyRoot);
}

template class SCCGraph<EmptyEdgeProperties, SCCVertexProperties>;