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

#include "ConstantWeightKShortestPathsFinder.h"

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
ConstantWeightKShortestPathsFinder::ConstantWeightKShortestPathsFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options), _pathAvailable(false) {}

ConstantWeightKShortestPathsFinder::~ConstantWeightKShortestPathsFinder() {}

// Sets up k-shortest-paths traversal from start to end
// Returns number of currently known paths
bool ConstantWeightKShortestPathsFinder::startKShortestPathsTraversal(
    arangodb::velocypack::Slice const& start, arangodb::velocypack::Slice const& end) {
  // TODO: ?
  if (start == end) {
    return true;
  }
  _start = arangodb::velocypack::StringRef(start);
  _end = arangodb::velocypack::StringRef(end);
  _pathAvailable = true;
  _shortestPaths.clear();

  TRI_IF_FAILURE("TraversalOOMInitialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return true;
}

bool ConstantWeightKShortestPathsFinder::computeShortestPath(
    const VertexRef& start, const VertexRef& end,
    const std::unordered_set<VertexRef>& forbiddenVertices,
    const std::unordered_set<Edge>& forbiddenEdges, Path& result) {
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

void ConstantWeightKShortestPathsFinder::computeNeighbourhoodOfVertex(
    VertexRef vertex, Direction direction, std::vector<VertexRef>& neighbours,
    std::vector<Edge>& edges) {
  std::unique_ptr<EdgeCursor> edgeCursor;

  switch (direction) {
    case BACKWARD:
      edgeCursor.reset(_options.nextReverseCursor(vertex));
      break;
    case FORWARD:
      edgeCursor.reset(_options.nextCursor(vertex));
      break;
    default:
      TRI_ASSERT(true);
  }

  auto callback = [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorIdx) -> void {
    if (edge.isString()) {
      if (edge.compareString(vertex.data(), vertex.length()) != 0) {
        VertexRef id = _options.cache()->persistString(VertexRef(edge));
        edges.emplace_back(std::move(eid));
        neighbours.emplace_back(id);
      }
    } else {
      VertexRef other(transaction::helpers::extractFromFromDocument(edge));
      if (other == vertex) {
        other = VertexRef(transaction::helpers::extractToFromDocument(edge));
      }
      if (other != vertex) {
        VertexRef id = _options.cache()->persistString(other);
        edges.emplace_back(std::move(eid));
        neighbours.emplace_back(id);
      }
    }
  };
  edgeCursor->readAll(callback);
}

bool ConstantWeightKShortestPathsFinder::advanceFrontier(
    Ball& source, const Ball& target, const std::unordered_set<VertexRef>& forbiddenVertices,
    const std::unordered_set<Edge>& forbiddenEdges, VertexRef& join) {
  std::vector<VertexRef> neighbours;
  std::vector<graph::EdgeDocumentToken> edges;
  Frontier newFrontier;

  auto isEdgeForbidden = [forbiddenEdges](const Edge& e) -> bool {
    return forbiddenEdges.find(e) != forbiddenEdges.end();
  };
  auto isVertexForbidden = [forbiddenVertices](const VertexRef& v) -> bool {
    return forbiddenVertices.find(v) != forbiddenVertices.end();
  };

  for (auto& v : source._frontier) {
    neighbours.clear();
    edges.clear();

    computeNeighbourhoodOfVertex(v, source._direction, neighbours, edges);
    TRI_ASSERT(edges.size() == neighbours.size());
    size_t const neighboursSize = neighbours.size();
    for (size_t i = 0; i < neighboursSize; ++i) {
      if (!isEdgeForbidden(edges[i]) && !isVertexForbidden(neighbours[i])) {
        auto const& n = neighbours[i];

        // NOTE: _edges[i] stays intact after move
        // and is reset to a nullptr. So if we crash
        // here no mem-leaks. or undefined behavior
        // Just make sure _edges is not used after
        auto inserted =
            source._vertices.emplace(n, FoundVertex(v, std::move(edges[i])));

        // vertex was new
        if (inserted.second) {
          auto found = target._vertices.find(n);
          if (found != target._vertices.end()) {
            join = n;
            return true;
          }
          newFrontier.emplace_back(n);
        }
      }
    }
  }
  source._frontier = newFrontier;
  return false;
}

void ConstantWeightKShortestPathsFinder::reconstructPath(const Ball& left, const Ball& right,
                                                         const VertexRef& join,
                                                         Path& result) {
  result.clear();

  result._vertices.emplace_back(join);
  auto it = left._vertices.find(join);
  VertexRef next;
  while (it != left._vertices.end() && !it->second._startOrEnd) {
    next = it->second._pred;
    result._vertices.push_front(next);
    result._edges.push_front(it->second._edge);
    it = left._vertices.find(next);
  }
  it = right._vertices.find(join);
  while (it != right._vertices.end() && !it->second._startOrEnd) {
    next = it->second._pred;
    result._vertices.emplace_back(next);
    result._edges.emplace_back(it->second._edge);
    it = right._vertices.find(next);
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

bool ConstantWeightKShortestPathsFinder::computeNextShortestPath(Path& result) {
  std::unordered_set<VertexRef> forbiddenVertices;
  std::unordered_set<Edge> forbiddenEdges;
  std::vector<Path> candidates;
  Path tmpPath, candidate;
  TRI_ASSERT(!_shortestPaths.empty());
  auto& lastShortestPath = _shortestPaths.back();
  bool available = false;

  for (size_t i = 0; i < lastShortestPath.length() - 1; ++i) {
    auto& spur = lastShortestPath._vertices.at(i);

    forbiddenVertices.clear();
    forbiddenEdges.clear();

    // Must not use vertices on the prefix
    for (size_t j = 0; j < i; ++j) {
      forbiddenVertices.emplace(lastShortestPath._vertices[j]);
    }

    // TODO: This can be done more efficiently by storing shortest
    //       paths in a prefix/postfix tree
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
      candidates.emplace_back(candidate);
    }
  }

  if (!candidates.empty()) {
    std::sort(candidates.begin(), candidates.end(), [](const Path& p1, const Path& p2) {
      return p1._vertices.size() < p2._vertices.size();
    });

    auto const& p = candidates.front();
    result.clear();
    result.append(p, 0, p.length() - 1);
    available = true;
  }
  return available;
}

bool ConstantWeightKShortestPathsFinder::getNextPath(arangodb::graph::ShortestPathResult& result) {
  bool available = false;
  Path kShortestPath;

  if (_shortestPaths.empty()) {
    available = computeShortestPath(_start, _end, {}, {}, kShortestPath);
  } else {
    available = computeNextShortestPath(kShortestPath);
  }

  if (available) {
    _shortestPaths.emplace_back(kShortestPath);

    result.clear();
    result._vertices = kShortestPath._vertices;
    // WHUPS. Fix.
    for (auto& e : kShortestPath._edges) {
      result._edges.emplace_back(std::move(e));
    }

    _options.fetchVerticesCoordinator(result._vertices);

    TRI_IF_FAILURE("TraversalOOMPath") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  _pathAvailable = available;
  return available;
}

bool ConstantWeightKShortestPathsFinder::getNextPathAql(arangodb::velocypack::Builder& result) {
  ShortestPathResult path;

  if (getNextPath(path)) {
    result.clear();
    result.openObject();

    result.add(VPackValue("edges"));
    result.openArray();
    for (auto const& it : path._edges) {
      // TRI_ASSERT(it != nullptr);
      _options.cache()->insertEdgeIntoResult(it, result);
    }
    result.close();  // Array

    result.add(VPackValue("vertices"));
    result.openArray();
    for (auto const& it : path._vertices) {
      _options.cache()->insertVertexIntoResult(it, result);
    }
    result.close();  // Array

    result.close();  // Object
    TRI_ASSERT(result.isClosed());
    return true;
  } else {
    return false;
  }
}
