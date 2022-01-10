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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "BreadthFirstEnumerator.h"

#include "Aql/PruneExpressionEvaluator.h"
#include "Containers/Helpers.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

BreadthFirstEnumerator::PathStep::PathStep(std::string_view vertex)
    : sourceIdx(0), edge(EdgeDocumentToken()), vertex(vertex) {}

BreadthFirstEnumerator::PathStep::PathStep(size_t sourceIdx,
                                           EdgeDocumentToken&& edge,
                                           std::string_view vertex)
    : sourceIdx(sourceIdx), edge(edge), vertex(vertex) {}

BreadthFirstEnumerator::BreadthFirstEnumerator(Traverser* traverser,
                                               TraverserOptions* opts)
    : PathEnumerator(traverser, opts),
      _schreierIndex(0),
      _lastReturned(0),
      _currentDepth(0),
      _toSearchPos(0) {}

BreadthFirstEnumerator::~BreadthFirstEnumerator() {
  _opts->resourceMonitor().decreaseMemoryUsage(_schreier.capacity() *
                                               pathStepSize());
}

void BreadthFirstEnumerator::clear() {
  _schreier.clear();
  _schreierIndex = 0;
  _lastReturned = 0;
  _nextDepth.clear();
  _toSearch.clear();
  _currentDepth = 0;
  _toSearchPos = 0;
}

void BreadthFirstEnumerator::setStartVertex(std::string_view startVertex) {
  PathEnumerator::setStartVertex(startVertex);

  clear();

  growStorage();
  _schreier.emplace_back(startVertex);
  _toSearch.emplace_back(NextStep(0));
}

bool BreadthFirstEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (shouldPrune()) {
      TRI_ASSERT(_toSearch.size() == 1);
      // Throw the next one away
      _toSearch.clear();
    }
    // We have faked the 0 position in schreier for pruning
    _schreierIndex++;

    if (_opts->minDepth == 0) {
      if (_opts->usesPostFilter()) {
        auto evaluator = _opts->getPostFilterEvaluator();
        if (usePostFilter(evaluator)) {
          return true;
        }
      } else {
        return true;
      }
    }
  }
  if (_opts->maxDepth == 0) {
    // Short circuit.
    // We cannot find any path of length 0 or less
    return false;
  }
  _lastReturned++;

  // Avoid large call stacks.
  // Loop will be left if we are either finished
  // with searching.
  // Or we found vertices in the next depth for
  // a vertex.
  while (true) {
    while (_lastReturned < _schreierIndex) {
      // We still have something on our stack.
      // Paths have been read but not returned.

      if (_opts->usesPostFilter()) {
        auto evaluator = _opts->getPostFilterEvaluator();
        if (!usePostFilter(evaluator)) {
          // Skip this path, the Filter does not allow it.
          _lastReturned++;
          continue;
        }
      }
      // If we get here we now have a valid path to be returned
      return true;
    }

    if (_toSearchPos >= _toSearch.size()) {
      // This depth is done. GoTo next
      if (!prepareSearchOnNextDepth()) {
        // That's it. we are done.
        return false;
      }
    }
    // This access is always safe.
    // If not it should have bailed out before.
    TRI_ASSERT(_toSearchPos < _toSearch.size());

    auto const nextIdx = _toSearch[_toSearchPos++].sourceIdx;
    auto const nextVertex = _schreier[nextIdx].vertex;

    EdgeCursor* cursor = getCursor(nextVertex, _currentDepth);

    TRI_ASSERT(cursor != nullptr);
    cursor->readAll([&](graph::EdgeDocumentToken&& eid, VPackSlice e,
                        size_t cursorIdx) -> void {
      if (!keepEdge(eid, e, nextVertex, _currentDepth, cursorIdx)) {
        return;
      }

      if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
        if (pathContainsEdge(nextIdx, eid)) {
          // This edge is on the path.
          return;
        }
      }

      std::string_view vId;

      if (_traverser->getSingleVertex(e, nextVertex, _currentDepth + 1, vId)) {
        if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
          if (pathContainsVertex(nextIdx, vId)) {
            // This vertex is on the path.
            return;
          }
        }

        if (!validDisjointPath(nextIdx, vId)) {
          return;
        }

        growStorage();
        TRI_ASSERT(_schreier.capacity() > _schreier.size());
        _schreier.emplace_back(nextIdx, std::move(eid), vId);
        if (_currentDepth < _opts->maxDepth - 1) {
          // Prune here
          if (!shouldPrune()) {
            _nextDepth.emplace_back(_schreierIndex);
          }
        }
        _schreierIndex++;
      }
    });

    incHttpRequests(cursor->httpRequests());

    _opts->isQueryKilledCallback();
    // If we found a valid path, the schreier index was moved forward
    // Otherwise the searchPosition was moved forward.

    if (_currentDepth + 1 < _opts->minDepth) {
      // The depth we are working on is not supposed to be returned.
      // Move it fast-forward
      _lastReturned = _schreierIndex;
    }
  }
}

arangodb::aql::AqlValue BreadthFirstEnumerator::lastVertexToAqlValue() {
  return vertexToAqlValue(_lastReturned);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::lastEdgeToAqlValue() {
  return edgeToAqlValue(_lastReturned);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::pathToAqlValue(
    arangodb::velocypack::Builder& result) {
  return pathToIndexToAqlValue(result, _lastReturned);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::vertexToAqlValue(size_t index) {
  TRI_ASSERT(index < _schreier.size());
  return _traverser->fetchVertexData(_schreier[index].vertex);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::edgeToAqlValue(size_t index) {
  TRI_ASSERT(index < _schreier.size());
  if (index == 0) {
    // This is the first Vertex. No Edge Pointing to it
    return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  }
  return _opts->cache()->fetchEdgeAqlResult(_schreier[index].edge);
}

VPackSlice BreadthFirstEnumerator::pathToIndexToSlice(VPackBuilder& result,
                                                      size_t index,
                                                      bool fromPrune) {
  _tempPathHelper.clear();
  while (index != 0) {
    // Walk backwards through the path and push everything found on the local
    // stack
    _tempPathHelper.emplace_back(index);
    index = _schreier[index].sourceIdx;
  }

  result.clear();
  result.openObject();
  if (fromPrune || _opts->producePathsEdges()) {
    result.add(StaticStrings::GraphQueryEdges,
               VPackValue(VPackValueType::Array));
    for (auto it = _tempPathHelper.rbegin(); it != _tempPathHelper.rend();
         ++it) {
      _opts->cache()->insertEdgeIntoResult(_schreier[*it].edge, result);
    }
    result.close();  // edges
  }
  if (fromPrune || _opts->producePathsVertices()) {
    result.add(StaticStrings::GraphQueryVertices,
               VPackValue(VPackValueType::Array));
    // Always add the start vertex
    _traverser->addVertexToVelocyPack(_schreier[0].vertex, result);
    for (auto it = _tempPathHelper.rbegin(); it != _tempPathHelper.rend();
         ++it) {
      _traverser->addVertexToVelocyPack(_schreier[*it].vertex, result);
    }
    result.close();  // vertices
  }
  result.close();
  TRI_ASSERT(result.isClosed());
  return result.slice();
}

arangodb::aql::AqlValue BreadthFirstEnumerator::pathToIndexToAqlValue(
    arangodb::velocypack::Builder& result, size_t index) {
  return arangodb::aql::AqlValue(pathToIndexToSlice(result, index, false));
}

bool BreadthFirstEnumerator::pathContainsVertex(size_t index,
                                                std::string_view vertex) const {
  while (true) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    if (step.vertex == vertex) {
      // We have the given vertex on this path
      return true;
    }
    if (index == 0) {
      // We have checked the complete path
      return false;
    }
    index = step.sourceIdx;
  }
}

bool BreadthFirstEnumerator::pathContainsEdge(
    size_t index, graph::EdgeDocumentToken const& edge) const {
  while (index != 0) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    if (step.edge.equals(edge)) {
      // We have the given edge on this path
      return true;
    }
    index = step.sourceIdx;
  }
  // We have checked the complete path
  return false;
}

bool BreadthFirstEnumerator::prepareSearchOnNextDepth() {
  if (_nextDepth.empty()) {
    // Nothing left to search
    return false;
  }
  // Save copies:
  // We clear current
  // we swap current and next.
  // So now current is filled
  // and next is empty.
  _toSearch.clear();
  _toSearchPos = 0;
  _toSearch.swap(_nextDepth);
  _currentDepth++;
  TRI_ASSERT(_toSearchPos < _toSearch.size());
  TRI_ASSERT(_nextDepth.empty());
  TRI_ASSERT(_currentDepth < _opts->maxDepth);
  return true;
}

bool BreadthFirstEnumerator::shouldPrune() {
  if (!_opts->usesPrune()) {
    return false;
  }

  transaction::BuilderLeaser pathBuilder(_opts->trx());

  // evaluator->evaluate() might access these, so they have to live long enough.
  aql::AqlValue vertex, edge;
  aql::AqlValueGuard vertexGuard{vertex, true}, edgeGuard{edge, true};

  auto* evaluator = _opts->getPruneEvaluator();
  TRI_ASSERT(evaluator != nullptr);

  if (evaluator->needsVertex()) {
    // Note: vertexToAqlValue() copies the original vertex into the AqlValue.
    // This could be avoided with a function that just returns the slice,
    // as it will stay valid long enough.
    vertex = vertexToAqlValue(_schreierIndex);
    evaluator->injectVertex(vertex.slice());
  }
  if (evaluator->needsEdge()) {
    // Note: edgeToAqlValue() copies the original edge into the AqlValue.
    // This could be avoided with a function that just returns the slice,
    // as it will stay valid long enough.
    edge = edgeToAqlValue(_schreierIndex);
    evaluator->injectEdge(edge.slice());
  }
  if (evaluator->needsPath()) {
    VPackSlice path =
        pathToIndexToSlice(*pathBuilder.get(), _schreierIndex, true);
    evaluator->injectPath(path);
  }
  return evaluator->evaluate();
}

void BreadthFirstEnumerator::growStorage() {
  size_t capacity = arangodb::containers::Helpers::nextCapacity(_schreier, 8);

  if (capacity > _schreier.capacity()) {
    arangodb::ResourceUsageScope guard(
        _opts->resourceMonitor(),
        (capacity - _schreier.capacity()) * pathStepSize());

    _schreier.reserve(capacity);

    // now we are responsible for tracking the memory
    guard.steal();
  }
}

constexpr size_t BreadthFirstEnumerator::pathStepSize() const noexcept {
  return sizeof(void*) + sizeof(PathStep) + 2 * sizeof(NextStep);
}

#ifndef USE_ENTERPRISE
bool BreadthFirstEnumerator::validDisjointPath(
    size_t /*index*/, std::string_view const& /*vertex*/) const {
  return true;
}
#endif
