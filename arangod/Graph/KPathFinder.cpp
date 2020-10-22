////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "KPathFinder.h"
#include "Graph/EdgeCursor.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"

#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

KPathFinder::PathResult::PathResult(size_t numItems) {
  _vertices.reserve(numItems);
  _edges.reserve(numItems);
}

auto KPathFinder::PathResult::clear() -> void {
  _vertices.clear();
  _edges.clear();
  _uniqueVertices.clear();
}

auto KPathFinder::PathResult::appendVertex(VertexRef v) -> void {
  _vertices.push_back(v);
  _uniqueVertices.emplace(v);
}

auto KPathFinder::PathResult::prependVertex(VertexRef v) -> void {
  _vertices.insert(_vertices.begin(), v);
  _uniqueVertices.emplace(v);
}

auto KPathFinder::PathResult::appendEdge(EdgeDocumentToken e) -> void {
  _edges.push_back(e);
}

auto KPathFinder::PathResult::prependEdge(EdgeDocumentToken e) -> void {
  _edges.insert(_edges.begin(), e);
}

auto KPathFinder::PathResult::toVelocyPack(ShortestPathOptions& options,
                                           arangodb::velocypack::Builder& builder) -> void {
  auto cache = options.cache();
  TRI_ASSERT(cache != nullptr);

  options.fetchVerticesCoordinator(_vertices);
  VPackObjectBuilder path{&builder};
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    for (auto const& v : _vertices) {
      cache->insertVertexIntoResult(v.stringRef(), builder);
    }
  }

  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    for (auto const& e : _edges) {
      cache->insertEdgeIntoResult(e, builder);
    }
  }
}

auto KPathFinder::PathResult::isValid() const -> bool {
  return _uniqueVertices.size() == _vertices.size();
}

bool KPathFinder::VertexIdentifier::operator<(VertexIdentifier const& other) const noexcept {
  // We only compare on the id vaue.
  // predecessor does not matter
  return id < other.id;
}

KPathFinder::Ball::Ball(Direction dir, ShortestPathOptions& opts)
    : _direction(dir), _cache(opts.cache()), _minDepth{opts.minDepth} {
  TRI_ASSERT(_cache != nullptr);
  _cursor = opts.buildCursor(dir == Direction::BACKWARD);
}

KPathFinder::Ball::~Ball() = default;

void KPathFinder::Ball::clear() {
  _shell.clear();
  _interior.clear();
  _depth = 0;
  _searchIndex = std::numeric_limits<size_t>::max();
}

void KPathFinder::Ball::reset(VertexRef center) {
  clear();
  _center = center;
  _shell.emplace(VertexIdentifier{center, 0, EdgeDocumentToken{}});
}

void KPathFinder::Ball::startNextDepth() {
  // Move everything from Shell to interior
  // Now Shell will contain the new vertices
  _searchIndex = _interior.size();
  _interior.insert(_interior.end(), std::make_move_iterator(_shell.begin()),
                   std::make_move_iterator(_shell.end()));
  _shell.clear();
  _depth++;
}

auto KPathFinder::Ball::noPathLeft() const -> bool {
  return doneWithDepth() && _shell.empty();
}

auto KPathFinder::Ball::doneWithDepth() const -> bool {
  return _searchIndex >= _interior.size();
}

auto KPathFinder::Ball::getDepth() const -> size_t { return _depth; }

auto KPathFinder::Ball::shellSize() const -> size_t { return _shell.size(); }

auto KPathFinder::Ball::buildPath(VertexIdentifier const& vertexInShell,
                                  PathResult& path) -> void {
  VertexIdentifier const* myVertex = &vertexInShell;
  if (_direction == Direction::FORWARD) {
    while (myVertex->predecessor != 0 || myVertex->id != _center) {
      path.prependVertex(myVertex->id);
      path.prependEdge(myVertex->edge);
      TRI_ASSERT(_interior.size() > myVertex->predecessor);
      myVertex = &_interior[myVertex->predecessor];
    }
    path.prependVertex(_center);
  } else {
    // For backward we just need to attach ourself
    // So everything until here should be done.
    // TRI_ASSERT(!path.empty());
    if (myVertex->predecessor == 0 && myVertex->id == _center) {
      // already reached the center
      return;
    }
    TRI_ASSERT(_interior.size() > myVertex->predecessor);
    path.appendEdge(myVertex->edge);
    myVertex = &_interior[myVertex->predecessor];
    while (myVertex->predecessor != 0 || myVertex->id != _center) {
      path.appendVertex(myVertex->id);
      path.appendEdge(myVertex->edge);
      TRI_ASSERT(_interior.size() > myVertex->predecessor);
      myVertex = &_interior[myVertex->predecessor];
    }
    path.appendVertex(_center);
  }
}

auto KPathFinder::Ball::matchResultsInShell(VertexIdentifier const& match,
                                            ResultList& results) const -> void {
  auto [first, last] = _shell.equal_range(match);
  if (_direction == FORWARD) {
    while (first != last) {
      results.push_back(std::make_pair(*first, match));
      first++;
    }
  } else {
    while (first != last) {
      results.push_back(std::make_pair(match, *first));
      first++;
    }
  }
}

auto KPathFinder::Ball::computeNeighbourhoodOfNextVertex(Ball const& other,
                                                         ResultList& results) -> void {
  TRI_ASSERT(!doneWithDepth());
  auto const& vertex = _interior[_searchIndex].id;
  _cursor->rearm(vertex.stringRef(), 0);
  _cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorIdx*/) -> void {
    VertexRef id = _cache->persistString(([&]() -> auto {
      if (edge.isString()) {
        return VertexRef(edge);
      } else {
        VertexRef other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex) {
          other = VertexRef(transaction::helpers::extractToFromDocument(edge));
        }
        return other;
      }
    })());

    VertexIdentifier match{id, _searchIndex, std::move(eid)};
    if (getDepth() + other.getDepth() >= _minDepth) {
      other.matchResultsInShell(match, results);
    }
    _shell.emplace(std::move(match));
  });
  ++_searchIndex;
}

KPathFinder::KPathFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options), 
      _left{Direction::FORWARD, options}, 
      _right{Direction::BACKWARD, options},
      _resultPath(std::min<size_t>(16, options.maxDepth)) {
  _results.reserve(8);
}

KPathFinder::~KPathFinder() = default;

void KPathFinder::reset(VertexRef source, VertexRef target) {
  _results.clear();
  _left.reset(source);
  _right.reset(target);
  _resultPath.clear();

  // Special Case depth == 0
  if (options().minDepth == 0 && source == target) {
    _results.emplace_back(
        std::make_pair(VertexIdentifier{source, 0, EdgeDocumentToken{}},
                       VertexIdentifier{target, 0, EdgeDocumentToken{}}));
  }
}

void KPathFinder::clear() { _results.clear(); }

bool KPathFinder::shortestPath(arangodb::velocypack::Slice const&,
                               arangodb::velocypack::Slice const&,
                               arangodb::graph::ShortestPathResult&) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto KPathFinder::isDone() const -> bool {
  return _results.empty() && searchDone();
}

// get the next available path serialized in the builder
auto KPathFinder::getNextPath(VPackBuilder& result) -> bool {
  while (!isDone()) {
    while (_results.empty() && !searchDone()) {
      if (_searchLeft) {
        if (ADB_UNLIKELY(_left.doneWithDepth())) {
          startNextDepth();
        } else {
          _left.computeNeighbourhoodOfNextVertex(_right, _results);
        }
      } else {
        if (ADB_UNLIKELY(_right.doneWithDepth())) {
          startNextDepth();
        } else {
          _right.computeNeighbourhoodOfNextVertex(_left, _results);
        }
      }
    }

    while (!_results.empty()) {
      auto [leftVertex, rightVertex] = _results.back();

      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);

      // result done
      _results.pop_back();
      if (_resultPath.isValid()) {
        _resultPath.toVelocyPack(options(), result);
        return true;
      }
    }
  }
  return false;
}

// get the next available path serialized in the builder
auto KPathFinder::skipPath() -> bool {
  while (!isDone()) {
    while (_results.empty() && !searchDone()) {
      if (_searchLeft) {
        if (ADB_UNLIKELY(_left.doneWithDepth())) {
          startNextDepth();
        } else {
          _left.computeNeighbourhoodOfNextVertex(_right, _results);
        }
      } else {
        if (ADB_UNLIKELY(_right.doneWithDepth())) {
          startNextDepth();
        } else {
          _right.computeNeighbourhoodOfNextVertex(_left, _results);
        }
      }
    }
    while (!_results.empty()) {
      auto [leftVertex, rightVertex] = _results.back();

      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);

      // result done
      _results.pop_back();
      if (_resultPath.isValid()) {
        return true;
      }
    }
  }
  return false;
}

auto KPathFinder::startNextDepth() -> void {
  if (_right.shellSize() < _left.shellSize()) {
    _searchLeft = false;
    _right.startNextDepth();
  } else {
    _searchLeft = true;
    _left.startNextDepth();
  }
}

auto KPathFinder::searchDone() const -> bool {
  return _left.noPathLeft() || _right.noPathLeft() ||
         _left.getDepth() + _right.getDepth() > options().maxDepth;
}
