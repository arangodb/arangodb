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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PathEnumerator.h"
#include "Aql/AqlValue.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeCursor.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

using namespace arangodb;

using PathEnumerator = arangodb::traverser::PathEnumerator;
using DepthFirstEnumerator = arangodb::traverser::DepthFirstEnumerator;
using Traverser = arangodb::traverser::Traverser;
using TraverserOptions = arangodb::traverser::TraverserOptions;

namespace arangodb {
namespace traverser {

EnumeratedPath::EnumeratedPath(arangodb::ResourceMonitor& resourceMonitor)
  : _resourceMonitor(resourceMonitor) {}

EnumeratedPath::~EnumeratedPath() {
  size_t memoryUsage = 
      (_vertices.capacity() * sizeof(typename decltype(_vertices)::value_type)) +
      (_edges.capacity() * sizeof(typename decltype(_edges)::value_type));

  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
}

template <typename T>
void EnumeratedPath::growStorage(std::vector<T>& data) {
  size_t capacity;

  if (data.empty()) {
    // reserve some initial space
    capacity = 8;
  } else {
    capacity = data.size() + 1;
    // allocate with power of 2 growth
    if (capacity > data.capacity()) {
      capacity *= 2;
    }
  }

  TRI_ASSERT(capacity > data.size());
  
  if (capacity > data.capacity()) {
    // reserve space
    ResourceUsageScope guard(_resourceMonitor, (capacity - data.capacity()) * sizeof(T));
    // if this fails, we don't have to rollback anything
    data.reserve(capacity);

    // now we are responsible for tracking the memory usage
    guard.steal();
  }
}

void EnumeratedPath::pushVertex(arangodb::velocypack::StringRef const& v) {
  growStorage(_vertices);
  _vertices.emplace_back(v);
}

void EnumeratedPath::pushEdge(graph::EdgeDocumentToken const& e) {
  growStorage(_edges);
  _edges.emplace_back(e);
}

void EnumeratedPath::popVertex() noexcept {
  TRI_ASSERT(!_vertices.empty());
  _vertices.pop_back();
}

void EnumeratedPath::popEdge() noexcept { 
  TRI_ASSERT(!_edges.empty());
  _edges.pop_back();
}

void EnumeratedPath::clear() {
  _vertices.clear();
  _edges.clear();
}

size_t EnumeratedPath::numVertices() const noexcept { return _vertices.size(); }

size_t EnumeratedPath::numEdges() const noexcept { return _edges.size(); }

std::vector<arangodb::velocypack::StringRef> const& EnumeratedPath::vertices() const noexcept { return _vertices; }

std::vector<graph::EdgeDocumentToken> const& EnumeratedPath::edges() const noexcept { return _edges; }

arangodb::velocypack::StringRef const& EnumeratedPath::lastVertex() const noexcept {
  TRI_ASSERT(!_vertices.empty());
  return _vertices.back();
}

graph::EdgeDocumentToken const& EnumeratedPath::lastEdge() const noexcept {
  TRI_ASSERT(!_edges.empty());
  return _edges.back();
}

} // namespace
} // namespace

PathEnumerator::PathEnumerator(Traverser* traverser, TraverserOptions* opts)
    : _traverser(traverser), 
      _opts(opts),
      _enumeratedPath(_opts->resourceMonitor()),
      _httpRequests(0),
      _isFirst(true) {}

PathEnumerator::~PathEnumerator() = default;

void PathEnumerator::setStartVertex(arangodb::velocypack::StringRef startVertex) {
  _isFirst = true;
  _httpRequests = 0;

  _enumeratedPath.clear();
  _enumeratedPath.pushVertex(startVertex);
}

bool PathEnumerator::keepEdge(arangodb::graph::EdgeDocumentToken& eid,
                              arangodb::velocypack::Slice edge,
                              arangodb::velocypack::StringRef sourceVertex,
                              size_t depth, size_t cursorId) {
  if (_opts->hasEdgeFilter(depth, cursorId)) {
    VPackSlice e = edge;
    if (edge.isString()) {
      e = _opts->cache()->lookupToken(eid);
    }
    if (!_traverser->edgeMatchesConditions(e, sourceVertex, depth, cursorId)) {
      // This edge does not pass the filtering
      return false;
    }
  }

  return _opts->destinationCollectionAllowed(edge, sourceVertex);
}

graph::EdgeCursor* PathEnumerator::getCursor(arangodb::velocypack::StringRef nextVertex,
                                             uint64_t currentDepth) {
  if (currentDepth >= _cursors.size()) {
    _cursors.emplace_back(_opts->buildCursor(currentDepth));
  }
  graph::EdgeCursor* cursor = _cursors.at(currentDepth).get();
  cursor->rearm(nextVertex, currentDepth);
  return cursor;
}

DepthFirstEnumerator::DepthFirstEnumerator(Traverser* traverser, TraverserOptions* opts)
    : PathEnumerator(traverser, opts), _activeCursors(0), _pruneNext(false) {}

DepthFirstEnumerator::~DepthFirstEnumerator() = default;

void DepthFirstEnumerator::setStartVertex(arangodb::velocypack::StringRef startVertex) {
  PathEnumerator::setStartVertex(startVertex);

  _activeCursors = 0;
  _pruneNext = false;
}

bool DepthFirstEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (shouldPrune()) {
      _pruneNext = true;
    }
    if (_opts->minDepth == 0) {
      return true;
    }
  }
  if (_enumeratedPath.numVertices() == 0) {
    // We are done;
    return false;
  }

  while (true) {
    if (_enumeratedPath.numEdges() < _opts->maxDepth && !_pruneNext) {
      // We are not done with this path, so
      // we reserve the cursor for next depth
      graph::EdgeCursor* cursor =
          getCursor(arangodb::velocypack::StringRef(_enumeratedPath.lastVertex()),
                    _enumeratedPath.numEdges());
      incHttpRequests(cursor->httpRequests());
      ++_activeCursors;
    } else {
      if (_enumeratedPath.numEdges() > 0) {
        // This path is at the end. cut the last step
        _enumeratedPath.popVertex();
        _enumeratedPath.popEdge();
      }
    }
    _pruneNext = false;

    bool foundPath = false;

    auto callback = [&](graph::EdgeDocumentToken&& eid, VPackSlice const& edge, size_t cursorId) {
      if (!keepEdge(eid, edge,
                    arangodb::velocypack::StringRef(_enumeratedPath.lastVertex()),
                    _enumeratedPath.numEdges(), cursorId)) {
        return;
      }

      if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
        if (ServerState::instance()->isCoordinator()) {
          for (auto const& it : _enumeratedPath.edges()) {
            // We might already have this edge on the path.
            if (it.equalsCoordinator(eid)) {
              return;
            }
          }
        } else {
          for (auto const& it : _enumeratedPath.edges()) {
            if (it.equalsLocal(eid)) {
              return;
            }
          }
        }
      }

      // We have to check if edge and vertex is valid
      if (_traverser->getVertex(edge, _enumeratedPath)) {
        // case both are valid.
        if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
          auto& e = _enumeratedPath.lastVertex();
          bool foundOnce = false;
          for (auto const& it : _enumeratedPath.vertices()) {
            if (foundOnce) {
              foundOnce = false;  // if we leave with foundOnce == false we
              // found the vertex earlier
              break;
            }
            if (it == e) {
              foundOnce = true;
            }
          }
          if (!foundOnce) {
            // We found it and it was not the last element (expected)
            // This vertex is allready on the path
            _enumeratedPath.popVertex();
            return;
          }
        }

        _enumeratedPath.pushEdge(eid);
        foundPath = true;
      }
      // Vertex Invalid. Do neither insert edge nor vertex
    };

    while (_activeCursors > 0) {
      TRI_ASSERT(_activeCursors == _enumeratedPath.numEdges() + 1);
      auto& cursor = _cursors[_activeCursors - 1];

      if (cursor->next(callback)) {
        if (foundPath) {
          if (shouldPrune()) {
            _pruneNext = true;
          }
          if (_enumeratedPath.numEdges() < _opts->minDepth) {
            // We have a valid prefix, but do NOT return this path
            break;
          }
          return true;
        }
      } else {
        // cursor is empty.
        TRI_ASSERT(_activeCursors > 0);
        --_activeCursors;
        if (_enumeratedPath.numEdges() > 0) {
          _enumeratedPath.popEdge();
          _enumeratedPath.popVertex();
        }
      }
    }

    if (_activeCursors == 0) {
      // If we get here all cursors are exhausted.
      _enumeratedPath.clear();
      return false;
    }

    _opts->isQueryKilledCallback();
  }  // while (true)
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastVertexToAqlValue() {
  return _traverser->fetchVertexData(
      arangodb::velocypack::StringRef(_enumeratedPath.lastVertex()));
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastEdgeToAqlValue() {
  if (_enumeratedPath.numEdges() == 0) {
    return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  }
  return _opts->cache()->fetchEdgeAqlResult(_enumeratedPath.lastEdge());
}

VPackSlice DepthFirstEnumerator::pathToSlice(VPackBuilder& result) {
  result.clear();
  result.openObject();
  result.add(StaticStrings::GraphQueryEdges, VPackValue(VPackValueType::Array));
  for (auto const& it : _enumeratedPath.edges()) {
    // TRI_ASSERT(it != nullptr);
    _opts->cache()->insertEdgeIntoResult(it, result);
  }
  result.close();
  result.add(StaticStrings::GraphQueryVertices, VPackValue(VPackValueType::Array));
  for (auto const& it : _enumeratedPath.vertices()) {
    _traverser->addVertexToVelocyPack(VPackStringRef(it), result);
  }
  result.close();
  result.close();
  TRI_ASSERT(result.isClosed());
  return result.slice();
}

arangodb::aql::AqlValue DepthFirstEnumerator::pathToAqlValue(VPackBuilder& result) {
  return arangodb::aql::AqlValue(pathToSlice(result));
}

bool DepthFirstEnumerator::shouldPrune() {
  // We need to call prune here
  if (!_opts->usesPrune()) {
    return false;
  }

  transaction::BuilderLeaser pathBuilder(_opts->trx());
  // evaluator->evaluate() might access these, so they have to live long
  // enough
  aql::AqlValue vertex, edge;
  aql::AqlValueGuard vertexGuard{vertex, true}, edgeGuard{edge, true};

  aql::PruneExpressionEvaluator* evaluator = _opts->getPruneEvaluator();
  TRI_ASSERT(evaluator != nullptr);

  if (evaluator->needsVertex()) {
    vertex = lastVertexToAqlValue();
    evaluator->injectVertex(vertex.slice());
  }
  if (evaluator->needsEdge()) {
    edge = lastEdgeToAqlValue();
    evaluator->injectEdge(edge.slice());
  }
  if (evaluator->needsPath()) {
    VPackSlice path = pathToSlice(*pathBuilder.get());
    evaluator->injectPath(path);
  }
  return evaluator->evaluate();
}
