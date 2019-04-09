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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "NeighborsEnumerator.h"

#include "Aql/PruneExpressionEvaluator.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeCursor.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

NeighborsEnumerator::NeighborsEnumerator(Traverser* traverser, VPackSlice const& startVertex,
                                         TraverserOptions* opts)
    : PathEnumerator(traverser, startVertex.copyString(), opts), _searchDepth(0) {
  StringRef vId = _traverser->traverserCache()->persistString(StringRef(startVertex));
  _allFound.insert(vId);
  _currentDepth.insert(vId);
  _iterator = _currentDepth.begin();
}

bool NeighborsEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (shouldPrune(*_iterator)) {
      _toPrune.emplace(*_iterator);
    }
    if (_opts->minDepth == 0) {
      return true;
    }
  }

  if (_iterator == _currentDepth.end() || ++_iterator == _currentDepth.end()) {
    do {
      // This depth is done. Get next
      if (_opts->maxDepth == _searchDepth) {
        // We are finished.
        return false;
      }

      swapLastAndCurrentDepth();
      for (auto const& nextVertex : _lastDepth) {
        auto callback = [&](EdgeDocumentToken&& eid, VPackSlice other, size_t cursorId) {
          if (_opts->hasEdgeFilter(_searchDepth, cursorId)) {
            // execute edge filter
            VPackSlice edge = other;
            if (edge.isString()) {
              edge = _opts->cache()->lookupToken(eid);
            }
            if (!_traverser->edgeMatchesConditions(edge, nextVertex, _searchDepth, cursorId)) {
              // edge does not qualify
              return;
            }
          }

          // Counting should be done in readAll
          StringRef v;
          if (other.isString()) {
            v = _opts->cache()->persistString(StringRef(other));
          } else {
            TRI_ASSERT(other.isObject());
            VPackSlice tmp = transaction::helpers::extractFromFromDocument(other);
            if (tmp.compareString(nextVertex.data(), nextVertex.length()) == 0) {
              tmp = transaction::helpers::extractToFromDocument(other);
            }
            TRI_ASSERT(tmp.isString());
            v = _opts->cache()->persistString(StringRef(tmp));
          }

          if (_allFound.find(v) == _allFound.end()) {
            if (_traverser->vertexMatchesConditions(v, _searchDepth + 1)) {
              _allFound.emplace(v);
              if (shouldPrune(v)) {
                _toPrune.emplace(v);
              }
              _currentDepth.emplace(v);
            }
          } else {
            _opts->cache()->increaseFilterCounter();
          }
        };

        std::unique_ptr<arangodb::graph::EdgeCursor> cursor(
            _opts->nextCursor(_traverser->mmdr(), nextVertex, _searchDepth));
        cursor->readAll(callback);
      }
      if (_currentDepth.empty()) {
        // Nothing found. Cannot do anything more.
        return false;
      }
      ++_searchDepth;
    } while (_searchDepth < _opts->minDepth);
    _iterator = _currentDepth.begin();
  }
  TRI_ASSERT(_iterator != _currentDepth.end());
  return true;
}

arangodb::aql::AqlValue NeighborsEnumerator::lastVertexToAqlValue() {
  TRI_ASSERT(_iterator != _currentDepth.end());
  return _traverser->fetchVertexData(*_iterator);
}

arangodb::aql::AqlValue NeighborsEnumerator::lastEdgeToAqlValue() {
  // If we get here the optimizer decided we do NOT need edges.
  // But the Block asks for it.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

arangodb::aql::AqlValue NeighborsEnumerator::pathToAqlValue(arangodb::velocypack::Builder& result) {
  // If we get here the optimizer decided we do NOT need paths
  // But the Block asks for it.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void NeighborsEnumerator::swapLastAndCurrentDepth() {
  // Filter all in _toPrune
  if (!_toPrune.empty()) {
    for (auto const& it : _toPrune) {
      _currentDepth.erase(it);
    }
    _toPrune.clear();
  }
  _lastDepth.swap(_currentDepth);
  _currentDepth.clear();
}

bool NeighborsEnumerator::shouldPrune(arangodb::StringRef v) {
  // Prune here
  if (!_opts->usesPrune()) {
    return false;
  }
  auto* evaluator = _opts->getPruneEvaluator();
  aql::AqlValue val;
  aql::AqlValueGuard guard{val, true};
  if (evaluator->needsVertex()) {
    val = _traverser->fetchVertexData(v);
    evaluator->injectVertex(val.slice());
  }
  // We cannot support these two here
  TRI_ASSERT(!evaluator->needsEdge());
  TRI_ASSERT(!evaluator->needsPath());
  return evaluator->evaluate();
}
