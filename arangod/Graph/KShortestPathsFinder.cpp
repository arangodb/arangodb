////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "KShortestPathsFinder.h"

#include "Aql/AqlValue.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>


using namespace arangodb;
using namespace arangodb::graph;

//
KShortestPathsFinder::KShortestPathsFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options), _pathAvailable(false) {}
KShortestPathsFinder::~KShortestPathsFinder() {}

// Sets up k-shortest-paths traversal from start to end
bool KShortestPathsFinder::startKShortestPathsTraversal(
    arangodb::velocypack::Slice const& start, arangodb::velocypack::Slice const& end) {
  _start = arangodb::velocypack::StringRef(start);
  _end = arangodb::velocypack::StringRef(end);

  _pathAvailable = true;
  _shortestPaths.clear();
  _candidatePaths.clear();


  TRI_IF_FAILURE("TraversalOOMInitialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return true;
}

bool KShortestPathsFinder::computeShortestPath(VertexRef const& start, VertexRef const& end,
                                               std::unordered_set<VertexRef> const& forbiddenVertices,
                                               std::unordered_set<Edge> const& forbiddenEdges,
                                               Path& result) {
  bool found = false;
  Ball left(start, FORWARD);
  Ball right(end, BACKWARD);
  VertexRef join;

  result.clear();

  while (!left._frontier.empty() && !right._frontier.empty() && !found) {
    _options.isQueryKilledCallback();

    // Choose the smaller frontier to expand.
    if (left._frontier.size() < right._frontier.size()) {
      found = advanceFrontier(left, right, forbiddenVertices, forbiddenEdges, join);
    } else {
      found = advanceFrontier(right, left, forbiddenVertices, forbiddenEdges, join);
    }
  }
  if (found) {
    reconstructPath(left, right, join, result);
  }
  return found;
}

void KShortestPathsFinder::computeNeighbourhoodOfVertexCache(VertexRef vertex, Direction direction, std::vector<Step>*& res) {
  auto lookup = _vertexCache.emplace(vertex, FoundVertex(vertex)).first;
  auto& cache = lookup->second; // want to update the cached vertex in place

  switch (direction) {
  case BACKWARD:
    if (!cache._hasCachedInNeighbours) {
      computeNeighbourhoodOfVertex(vertex, direction, cache._inNeighbours);
      cache._hasCachedInNeighbours = true;
    }
    res = &cache._inNeighbours;
    break;
  case FORWARD:
    if (!cache._hasCachedOutNeighbours) {
      computeNeighbourhoodOfVertex(vertex, direction, cache._outNeighbours);
      cache._hasCachedOutNeighbours = true;
    }
    res = &cache._outNeighbours;
    break;
  default:
    TRI_ASSERT(false);
  }
}

void KShortestPathsFinder::computeNeighbourhoodOfVertex(VertexRef vertex, Direction direction,
                                                        std::vector<Step>& steps) {
  std::unique_ptr<EdgeCursor> edgeCursor;

  switch (direction) {
    case BACKWARD:
      edgeCursor.reset(_options.nextReverseCursor(vertex));
      break;
    case FORWARD:
      edgeCursor.reset(_options.nextCursor(vertex));
      break;
    default:
      TRI_ASSERT(false);
  }

  // TODO: This is a bit of a hack
  if (_options.useWeight()) {
    auto callback = [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorIdx) -> void {
      if (edge.isString()) {
        VPackSlice doc = _options.cache()->lookupToken(eid);
        double weight = _options.weightEdge(doc);
        if (edge.compareString(vertex.data(), vertex.length()) != 0) {
          VertexRef id = _options.cache()->persistString(VertexRef(edge));
          steps.emplace_back(std::move(eid), id, weight);
        }
      } else {
        VertexRef other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex) {
          other = VertexRef(transaction::helpers::extractToFromDocument(edge));
        }
        if (other != vertex) {
          VertexRef id = _options.cache()->persistString(other);
          steps.emplace_back(std::move(eid), id, _options.weightEdge(edge));
        }
      }
    };
    edgeCursor->readAll(callback);
  } else {
    auto callback = [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorIdx) -> void {
      if (edge.isString()) {
        if (edge.compareString(vertex.data(), vertex.length()) != 0) {
          VertexRef id = _options.cache()->persistString(VertexRef(edge));
          steps.emplace_back(std::move(eid), id, 1);
        }
      } else {
        VertexRef other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex) {
          other = VertexRef(transaction::helpers::extractToFromDocument(edge));
        }
        if (other != vertex) {
          VertexRef id = _options.cache()->persistString(other);
          steps.emplace_back(std::move(eid), id, 1);
        }
      }
    };
    edgeCursor->readAll(callback);
  }
}

bool KShortestPathsFinder::advanceFrontier(Ball& source, Ball const& target,
                                           std::unordered_set<VertexRef> const& forbiddenVertices,
                                           std::unordered_set<Edge> const& forbiddenEdges,
                                           VertexRef& join) {
  VertexRef vr;
  DijkstraInfo *v, *w;
  std::vector<Step>* neighbours;

  bool success = source._frontier.popMinimal(vr, v);
  TRI_ASSERT(v != nullptr);
  TRI_ASSERT(vr == v->_vertex);
  if (!success) {
    return false;
  }

  computeNeighbourhoodOfVertexCache(vr, source._direction, neighbours);
  TRI_ASSERT(neighbours != nullptr);

  for (auto& s : *neighbours) {
    if (forbiddenEdges.find(s._edge) == forbiddenEdges.end() &&
        forbiddenVertices.find(s._vertex) == forbiddenVertices.end()) {
      double weight = v->_weight + s._weight;

      auto lookup = source._frontier.find(s._vertex);
      if (lookup != nullptr) {
        if (lookup->_weight > weight) {
          source._frontier.lowerWeight(s._vertex, weight);
          lookup->_pred = vr;
          lookup->_edge = s._edge;
          lookup->_weight = weight;
        }
      } else {
        source._frontier.insert(s._vertex,
                                std::make_unique<DijkstraInfo>(s._vertex,
                                                               std::move(s._edge), vr, weight));
      }
    }
  }
  v->_done = true;

  w = target._frontier.find(v->_vertex);
  if (w != nullptr && w->_done) {
    join = v->_vertex;
    return true;
  }

  return false;
}

void KShortestPathsFinder::reconstructPath(Ball const& left, Ball const& right,
                                           VertexRef const& join, Path& result) {
  result.clear();
  TRI_ASSERT(!join.empty());
  result._vertices.emplace_back(join);

  DijkstraInfo* it;
  it = left._frontier.find(join);
  TRI_ASSERT(it != nullptr);
  double startToJoin = it->_weight;
  result._weight = startToJoin;
  while (it != nullptr && it->_weight > 0) {
    result._vertices.push_front(it->_pred);
    result._edges.push_front(it->_edge);
    result._weights.push_front(it->_weight);
    it = left._frontier.find(it->_pred);
  }
  // Initial vertex has weight 0
  result._weights.push_front(0);

  it = right._frontier.find(join);
  TRI_ASSERT(it != nullptr);
  double joinToEnd = it->_weight;
  result._weight += joinToEnd;
  while (it != nullptr && it->_weight > 0) {
    result._vertices.emplace_back(it->_pred);
    result._edges.emplace_back(it->_edge);
    it = right._frontier.find(it->_pred);
    TRI_ASSERT(it != nullptr);  // should run into 0 weight before
    result._weights.emplace_back(startToJoin + (joinToEnd - it->_weight));
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

bool KShortestPathsFinder::computeNextShortestPath(Path& result) {
  std::unordered_set<VertexRef> forbiddenVertices;
  std::unordered_set<Edge> forbiddenEdges;
  Path tmpPath, candidate;
  TRI_ASSERT(!_shortestPaths.empty());
  auto& lastShortestPath = _shortestPaths.back();
  bool available = false;

  for (size_t i = lastShortestPath._branchpoint; i < lastShortestPath.length() - 1; ++i) {
    auto& spur = lastShortestPath._vertices.at(i);

    forbiddenVertices.clear();
    forbiddenEdges.clear();

    // Must not use vertices on the prefix
    for (size_t j = 0; j < i; ++j) {
      forbiddenVertices.emplace(lastShortestPath._vertices[j]);
    }

    // TODO: This can be done more efficiently by storing shortest
    //       paths in a prefix/postfix tree
    // previous paths with same prefix must not be
    for (auto const& p : _shortestPaths) {
      bool eq = true;
      for (size_t e = 0; e < i; ++e) {
        if (!p._edges.at(e).equals(lastShortestPath._edges.at(e))) {
          eq = false;
          break;
        }
      }
      if (eq && (i < p._edges.size())) {
        forbiddenEdges.emplace(p._edges.at(i));
      }
    }

    if (computeShortestPath(spur, _end, forbiddenVertices, forbiddenEdges, tmpPath)) {
      candidate.clear();
      candidate.append(lastShortestPath, 0, i);
      candidate.append(tmpPath, 0, tmpPath.length() - 1);
      candidate._branchpoint = i;

      auto it = find_if(_candidatePaths.begin(), _candidatePaths.end(), [candidate](Path const& v) { return v._weight >= candidate._weight; } );
      if (!(*it == candidate)) {
        _candidatePaths.emplace(it, candidate);
      }
    }
  }

  if (!_candidatePaths.empty()) {
    auto const& p = _candidatePaths.front();
    result.clear();
    result.append(p, 0, p.length() - 1);
    result._branchpoint = p._branchpoint;
    _candidatePaths.pop_front();
    available = true;
  }
  return available;
}

bool KShortestPathsFinder::getNextPath(Path& result) {
  bool available = false;
  result.clear();

  // TODO: this looks a bit ugly
  if (_shortestPaths.empty()) {
    if (_start == _end) {
      TRI_ASSERT(!_start.empty());
      result._vertices.emplace_back(_start);
      result._weight = 0;
      available = true;
    } else {
      available = computeShortestPath(_start, _end, {}, {}, result);
      result._branchpoint = 0;
    }
  } else {
    if (_start == _end) {
      available = false;
    } else {
      available = computeNextShortestPath(result);
    }
  }

  if (available) {
    _shortestPaths.emplace_back(result);
    _options.fetchVerticesCoordinator(result._vertices);

    TRI_IF_FAILURE("TraversalOOMPath") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  _pathAvailable = available;
  return available;
}

bool KShortestPathsFinder::getNextPathShortestPathResult(ShortestPathResult& result) {
  Path path;

  result.clear();
  if (getNextPath(path)) {
    result._vertices = path._vertices;
    result._edges = path._edges;
    return true;
  } else {
    return false;
  }
}

bool KShortestPathsFinder::getNextPathAql(arangodb::velocypack::Builder& result) {
  Path path;

  if (getNextPath(path)) {
    result.clear();
    result.openObject();

    result.add(VPackValue("edges"));
    result.openArray();
    for (auto const& it : path._edges) {
      _options.cache()->insertEdgeIntoResult(it, result);
    }
    result.close();  // Array

    result.add(VPackValue("vertices"));
    result.openArray();
    for (auto const& it : path._vertices) {
      _options.cache()->insertVertexIntoResult(it, result);
    }
    result.close();  // Array
    if (_options.useWeight()) {
      result.add("weight", VPackValue(path._weight));
    } else {
      // If not using weight, weight is defined as 1 per edge
      result.add("weight", VPackValue(path._edges.size()));
    }
    result.close();  // Object
    TRI_ASSERT(result.isClosed());
    return true;
  } else {
    return false;
  }
}
