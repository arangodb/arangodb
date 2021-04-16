////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

KShortestPathsFinder::KShortestPathsFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options),
      _left(FORWARD),
      _right(BACKWARD) {
  // cppcheck-suppress *
  _forwardCursor = options.buildCursor(false);
  // cppcheck-suppress *
  _backwardCursor = options.buildCursor(true);
}

KShortestPathsFinder::~KShortestPathsFinder() = default;

void KShortestPathsFinder::clear() {
  _shortestPaths.clear();
  _candidatePaths.clear();
  _vertexCache.clear();
  _traversalDone = true;
}

// Sets up k-shortest-paths traversal from start to end
bool KShortestPathsFinder::startKShortestPathsTraversal(
    arangodb::velocypack::Slice const& start, arangodb::velocypack::Slice const& end) {
  TRI_ASSERT(start.isString());
  TRI_ASSERT(end.isString());
  _start = arangodb::velocypack::StringRef(start);
  _end = arangodb::velocypack::StringRef(end);

  _vertexCache.clear();
  _shortestPaths.clear();
  _candidatePaths.clear();

  _traversalDone = false;

  TRI_IF_FAILURE("Travefalse") { THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); }

  return true;
}

bool KShortestPathsFinder::computeShortestPath(VertexRef const& start, VertexRef const& end,
                                               VertexSet const& forbiddenVertices,
                                               EdgeSet const& forbiddenEdges,
                                               Path& result) {
  _left.reset(start);
  _right.reset(end);
  VertexRef join;

  result.clear();

  auto currentBest = std::optional<double>{};

  // We will not improve anymore if we have found a best path and the smallest
  // combined distance between left and right is bigger than that path
  while (!_left.done(currentBest) && !_right.done(currentBest)) {
    _options.isQueryKilledCallback();

    // Choose the smaller frontier to expand.
    if (!_left.done(currentBest) && (_left._frontier.size() < _right._frontier.size())) {
      advanceFrontier(_left, _right, forbiddenVertices, forbiddenEdges, join, currentBest);
    } else {
      advanceFrontier(_right, _left, forbiddenVertices, forbiddenEdges, join, currentBest);
    }
  }

  if (currentBest.has_value()) {
    reconstructPath(_left, _right, join, result);
    return true;
  }
  // No path found
  return false;
}

void KShortestPathsFinder::computeNeighbourhoodOfVertexCache(VertexRef vertex,
                                                             Direction direction,
                                                             std::vector<Step>*& res) {
  auto lookup = _vertexCache.try_emplace(vertex, FoundVertex(vertex)).first;
  auto& cache = lookup->second;  // want to update the cached vertex in place

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
  EdgeCursor* cursor =
      direction == BACKWARD ? _backwardCursor.get() : _forwardCursor.get();
  cursor->rearm(vertex, 0);

  // TODO: This is a bit of a hack
  if (_options.useWeight()) {
    cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
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
    });
  } else {
    cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
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
    });
  }
}

void KShortestPathsFinder::advanceFrontier(Ball& source, Ball const& target,
                                           VertexSet const& forbiddenVertices,
                                           EdgeSet const& forbiddenEdges,
                                           VertexRef& join,
                                           std::optional<double>& currentBest) {
  VertexRef vr;
  DijkstraInfo *v, *w;

  bool success = source._frontier.popMinimal(vr, v);
  TRI_ASSERT(v != nullptr);
  TRI_ASSERT(vr == v->_vertex);
  if (!success) {
    return;
  }

  std::vector<Step>* neighbours;
  computeNeighbourhoodOfVertexCache(vr, source._direction, neighbours);
  TRI_ASSERT(neighbours != nullptr);

  for (auto const& s : *neighbours) {
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
                                std::make_unique<DijkstraInfo>(s._vertex, std::move(s._edge),
                                                               vr, weight));
      }
    }
  }
  v->_done = true;
  source._closest = v->_weight;

  w = target._frontier.find(v->_vertex);
  if (w != nullptr && w->_done) {
    // The total weight of the found path
    double totalWeight = v->_weight + w->_weight;
    if (!currentBest.has_value() || totalWeight < currentBest.value()) {
      join = v->_vertex;
      currentBest = totalWeight;
    }
  }
}

void KShortestPathsFinder::reconstructPath(Ball const& left, Ball const& right,
                                           VertexRef const& join, Path& result) {
  result.clear();
  TRI_ASSERT(!join.empty());
  result._vertices.emplace_back(join);

  DijkstraInfo* it = left._frontier.find(join);
  TRI_ASSERT(it != nullptr);
  double startToJoin = it->weight();
  result._weight = startToJoin;
  while (it != nullptr && it->getKey() != left.center()) {
    result._vertices.push_front(it->_pred);
    result._edges.push_front(it->_edge);
    result._weights.push_front(it->_weight);
    it = left._frontier.find(it->_pred);
  }
  // Initial vertex has weight 0
  result._weights.push_front(0);

  it = right._frontier.find(join);
  TRI_ASSERT(it != nullptr);
  double joinToEnd = it->weight();
  result._weight += joinToEnd;
  while (it != nullptr && it->getKey() != right.center()) {
    result._vertices.emplace_back(it->_pred);
    result._edges.emplace_back(it->_edge);
    it = right._frontier.find(it->_pred);
    TRI_ASSERT(it != nullptr);  // should run into 0 weight before
    // cppcheck-suppress nullPointerRedundantCheck
    result._weights.emplace_back(startToJoin + (joinToEnd - it->_weight));
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

bool KShortestPathsFinder::computeNextShortestPath(Path& result) {
  VertexSet forbiddenVertices;
  EdgeSet forbiddenEdges;
  TRI_ASSERT(!_shortestPaths.empty());
  auto& lastShortestPath = _shortestPaths.back();
  bool available = false;

  for (size_t i = lastShortestPath._branchpoint; i + 1 < lastShortestPath.length(); ++i) {
    auto& spur = lastShortestPath._vertices[i];

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
      if (i >= p._edges.size()) {
        continue;
      }
      bool eq = true;
      for (size_t e = 0; e < i; ++e) {
        if (!p._edges[e].equals(lastShortestPath._edges.at(e))) {
          eq = false;
          break;
        }
      }
      if (eq) {
        forbiddenEdges.emplace(p._edges[i]);
      }
    }

    // abuse result variable for some intermediate calculations here...
    // the "real" result is only calculated at the very end of this method
    result.clear();
    if (computeShortestPath(spur, _end, forbiddenVertices, forbiddenEdges, result)) {
      _candidate.clear();
      _candidate.append(lastShortestPath, 0, i);
      _candidate.append(result, 0, result.length() - 1);
      _candidate._branchpoint = i;

      auto it = find_if(_candidatePaths.begin(), _candidatePaths.end(),
                        [this](Path const& v) {
                          return v._weight >= _candidate._weight;
                        });
      if (it == _candidatePaths.end() || !(*it == _candidate)) {
        _candidatePaths.emplace(it, std::move(_candidate));
      }
    }
  }
    
  result.clear();

  if (!_candidatePaths.empty()) {
    auto const& p = _candidatePaths.front();
    result.append(p, 0, p.length() - 1);
    result._branchpoint = p._branchpoint;
    _candidatePaths.pop_front();
    available = true;
  }
  return available;
}

bool KShortestPathsFinder::getNextPath(Path& result) {
  result.clear();

  // This is for the first time that getNextPath is called
  if (_shortestPaths.empty()) {
    if (_start == _end) {
      TRI_ASSERT(!_start.empty());
      result._vertices.emplace_back(_start);
      result._weight = 0;
    } else {
      // Compute the first shortest path (i.e. the shortest path
      // between _start and _end!)
      computeShortestPath(_start, _end, {}, {}, result);
      result._branchpoint = 0;
    }
  } else {
    // We must not have _start == _end here, because we handle _start == _end
    computeNextShortestPath(result);
  }

  if (result.length() > 0) {
    _shortestPaths.emplace_back(result);
    _options.fetchVerticesCoordinator(result._vertices);

    TRI_IF_FAILURE("TraversalOOMPath") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } else {
    // If we did not find a path, traversal is done.
    _traversalDone = true;
  }
  return !_traversalDone;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool KShortestPathsFinder::getNextPathShortestPathResult(ShortestPathResult& result) {
  _tempPath.clear();

  result.clear();
  if (getNextPath(_tempPath)) {
    result._vertices = _tempPath._vertices;
    result._edges = _tempPath._edges;
    return true;
  }

  return false;
}
#endif

bool KShortestPathsFinder::getNextPathAql(arangodb::velocypack::Builder& result) {
  _tempPath.clear();

  if (getNextPath(_tempPath)) {
    result.clear();
    result.openObject();

    result.add("edges", VPackValue(VPackValueType::Array));
    for (auto const& it : _tempPath._edges) {
      _options.cache()->insertEdgeIntoResult(it, result);
    }
    result.close();  // Array

    result.add("vertices", VPackValue(VPackValueType::Array));
    for (auto const& it : _tempPath._vertices) {
      _options.cache()->appendVertex(it, result);
    }
    result.close();  // Array
    if (_options.useWeight()) {
      result.add("weight", VPackValue(_tempPath._weight));
    } else {
      // If not using weight, weight is defined as 1 per edge
      result.add("weight", VPackValue(_tempPath._edges.size()));
    }
    result.close();  // Object
    TRI_ASSERT(result.isClosed());
    return true;
  }
  
  return false;
}

bool KShortestPathsFinder::skipPath() {
  _tempPath.clear();
  return getNextPath(_tempPath);
}
