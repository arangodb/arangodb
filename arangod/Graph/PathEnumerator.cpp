////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

PathEnumerator::PathEnumerator(Traverser* traverser, TraverserOptions* opts)
    : _traverser(traverser), 
      _opts(opts),
      _httpRequests(0),
      _isFirst(true) {}

PathEnumerator::~PathEnumerator() = default;

void PathEnumerator::setStartVertex(arangodb::velocypack::StringRef startVertex) {
  _enumeratedPath.edges.clear();
  _enumeratedPath.vertices.clear();
  _isFirst = true;
  _httpRequests = 0;

  _enumeratedPath.vertices.push_back(startVertex);
  TRI_ASSERT(_enumeratedPath.vertices.size() == 1);
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
  if (_enumeratedPath.vertices.empty()) {
    // We are done;
    return false;
  }

  while (true) {
    if (_enumeratedPath.edges.size() < _opts->maxDepth && !_pruneNext) {
      // We are not done with this path, so
      // we reserve the cursor for next depth
      graph::EdgeCursor* cursor =
          getCursor(arangodb::velocypack::StringRef(_enumeratedPath.vertices.back()),
                    _enumeratedPath.edges.size());
      incHttpRequests(cursor->httpRequests());
      ++_activeCursors;
    } else {
      if (!_enumeratedPath.edges.empty()) {
        // This path is at the end. cut the last step
        _enumeratedPath.vertices.pop_back();
        _enumeratedPath.edges.pop_back();
      }
    }
    _pruneNext = false;

    bool foundPath = false;

    auto callback = [&](graph::EdgeDocumentToken&& eid, VPackSlice const& edge, size_t cursorId) {
      if (!keepEdge(eid, edge,
                    arangodb::velocypack::StringRef(_enumeratedPath.vertices.back()),
                    _enumeratedPath.edges.size(), cursorId)) {
        return;
      }

      if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
        if (ServerState::instance()->isCoordinator()) {
          for (auto const& it : _enumeratedPath.edges) {
            // We might already have this edge on the path.
            if (it.equalsCoordinator(eid)) {
              return;
            }
          }
        } else {
          for (auto const& it : _enumeratedPath.edges) {
            if (it.equalsLocal(eid)) {
              return;
            }
          }
        }
      }

      // We have to check if edge and vertex is valid
      if (_traverser->getVertex(edge, _enumeratedPath.vertices)) {
        // case both are valid.
        if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
          auto& e = _enumeratedPath.vertices.back();
          bool foundOnce = false;
          for (auto const& it : _enumeratedPath.vertices) {
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
            _enumeratedPath.vertices.pop_back();
            return;
          }
        }

        _enumeratedPath.edges.push_back(std::move(eid));
        foundPath = true;
      }
      // Vertex Invalid. Do neither insert edge nor vertex
    };

    while (_activeCursors > 0) {
      TRI_ASSERT(_activeCursors == _enumeratedPath.edges.size() + 1);
      auto& cursor = _cursors[_activeCursors - 1];

      if (cursor->next(callback)) {
        if (foundPath) {
          if (shouldPrune()) {
            _pruneNext = true;
          }
          if (_enumeratedPath.edges.size() < _opts->minDepth) {
            // We have a valid prefix, but do NOT return this path
            break;
          }
          return true;
        }
      } else {
        // cursor is empty.
        TRI_ASSERT(_activeCursors > 0);
        --_activeCursors;
        if (!_enumeratedPath.edges.empty()) {
          _enumeratedPath.edges.pop_back();
          _enumeratedPath.vertices.pop_back();
        }
      }
    }

    if (_activeCursors == 0) {
      // If we get here all cursors are exhausted.
      _enumeratedPath.edges.clear();
      _enumeratedPath.vertices.clear();
      return false;
    }
  }  // while (true)
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastVertexToAqlValue() {
  return _traverser->fetchVertexData(
      arangodb::velocypack::StringRef(_enumeratedPath.vertices.back()));
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastEdgeToAqlValue() {
  if (_enumeratedPath.edges.empty()) {
    return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  }
  return _opts->cache()->fetchEdgeAqlResult(_enumeratedPath.edges.back());
}

VPackSlice DepthFirstEnumerator::pathToSlice(VPackBuilder& result) {
  result.clear();
  result.openObject();
  result.add(StaticStrings::GraphQueryEdges, VPackValue(VPackValueType::Array));
  for (auto const& it : _enumeratedPath.edges) {
    // TRI_ASSERT(it != nullptr);
    _opts->cache()->insertEdgeIntoResult(it, result);
  }
  result.close();
  result.add(StaticStrings::GraphQueryVertices, VPackValue(VPackValueType::Array));
  for (auto const& it : _enumeratedPath.vertices) {
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
