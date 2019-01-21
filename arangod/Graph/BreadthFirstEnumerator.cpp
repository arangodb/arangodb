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

#include "BreadthFirstEnumerator.h"

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

BreadthFirstEnumerator::PathStep::PathStep(StringRef const vertex)
    : sourceIdx(0), edge(EdgeDocumentToken()), vertex(vertex) {}

BreadthFirstEnumerator::PathStep::PathStep(size_t sourceIdx, EdgeDocumentToken&& edge,
                                           StringRef const vertex)
    : sourceIdx(sourceIdx), edge(edge), vertex(vertex) {}

BreadthFirstEnumerator::PathStep::~PathStep() {}

BreadthFirstEnumerator::PathStep::PathStep(PathStep& other)
    : sourceIdx(other.sourceIdx), edge(other.edge), vertex(other.vertex) {}

BreadthFirstEnumerator::BreadthFirstEnumerator(Traverser* traverser, VPackSlice startVertex,
                                               TraverserOptions* opts)
    : PathEnumerator(traverser, startVertex.copyString(), opts),
      _schreierIndex(1),
      _lastReturned(0),
      _currentDepth(0),
      _toSearchPos(0) {
  _schreier.reserve(32);
  StringRef startVId = _opts->cache()->persistString(StringRef(startVertex));

  _schreier.emplace_back(std::make_unique<PathStep>(startVId));
  _toSearch.emplace_back(NextStep(0));
}

BreadthFirstEnumerator::~BreadthFirstEnumerator() {}

bool BreadthFirstEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (_opts->minDepth == 0) {
      return true;
    }
  }
  _lastReturned++;

  if (_lastReturned < _schreierIndex) {
    // We still have something on our stack.
    // Paths have been read but not returned.
    return true;
  }

  if (_opts->maxDepth == 0) {
    // Short circuit.
    // We cannot find any path of length 0 or less
    return false;
  }
  // Avoid large call stacks.
  // Loop will be left if we are either finished
  // with searching.
  // Or we found vertices in the next depth for
  // a vertex.
  while (true) {
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

    _tmpEdges.clear();
    auto const nextIdx = _toSearch[_toSearchPos++].sourceIdx;
    auto const nextVertex = _schreier[nextIdx]->vertex;
    StringRef vId;

    std::unique_ptr<EdgeCursor> cursor(
        _opts->nextCursor(_traverser->mmdr(), nextVertex, _currentDepth));
    if (cursor != nullptr) {
      bool shouldReturnPath = _currentDepth + 1 >= _opts->minDepth;
      bool didInsert = false;

      auto callback = [&](graph::EdgeDocumentToken&& eid, VPackSlice e,
                          size_t cursorIdx) -> void {
        if (_opts->hasEdgeFilter(_currentDepth, cursorIdx)) {
          VPackSlice edge = e;
          if (edge.isString()) {
            edge = _opts->cache()->lookupToken(eid);
          }
          if (!_traverser->edgeMatchesConditions(edge, nextVertex, _currentDepth, cursorIdx)) {
            return;
          }
        }
        if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
          if (pathContainsEdge(nextIdx, eid)) {
            // This edge is on the path.
            return;
          }
        }

        if (_traverser->getSingleVertex(e, nextVertex, _currentDepth + 1, vId)) {
          if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
            if (pathContainsVertex(nextIdx, vId)) {
              // This vertex is on the path.
              return;
            }
          }

          _schreier.emplace_back(std::make_unique<PathStep>(nextIdx, std::move(eid), vId));
          if (_currentDepth < _opts->maxDepth - 1) {
            _nextDepth.emplace_back(NextStep(_schreierIndex));
          }
          _schreierIndex++;
          didInsert = true;
        }
      };

      cursor->readAll(callback);

      if (!shouldReturnPath) {
        _lastReturned = _schreierIndex;
        didInsert = false;
      }
      if (didInsert) {
        // We exit the loop here.
        // _schreierIndex is moved forward
        break;
      }
    }
    // Nothing found for this vertex.
    // _toSearchPos is increased so
    // we are not stuck in an endless loop
  }

  // _lastReturned points to the last used
  // entry. We compute the path to it.
  return true;
}

void BreadthFirstEnumerator::prune() {
  if (_nextDepth.empty()) {
    // Nothing to prune, we do not search any further
    return;
  }

  // We execute a binarySearch on the nextDepth
  // vector.
  // We make use of the following features:
  // 1. The elements in vector are sorted
  // 2. The distance between two adjacent
  //    entries is most likely 1
  // 3. The element is stored atMost once
  size_t upperBound = _nextDepth.size();
  size_t lowerBound = 0;
  if (_nextDepth[upperBound].sourceIdx < _lastReturned
      || _lastReturned < _nextDepth[lowerBound].sourceIdx) {
    // We do not have this vertex
    // short circuit.
    return;
  }
  size_t candidate = 0;
  while (true) {
    TRI_ASSERT(lowerBound <= upperBound);
    candidate = (upperBound + lowerBound) / 2;
    size_t toCompare = _nextDepth[candidate].sourceIdx;
    if (candidate == lowerBound) {
      // Only possible if they differ by 0 or 1
      TRI_ASSERT(upperBound - lowerBound < 2);
      // We do two more compares and we are either done
      // or give up.
      if (toCompare == _lastReturned) {
        // We are done, eliminate
        _nextDepth.erase(_nextDepth.begin() + candidate);
        return;
      }
      if (_nextDepth[upperBound].sourceIdx == _lastReturned) {
        // We are done, eliminate
        _nextDepth.erase(_nextDepth.begin() + upperBound);
        return;
      }
      // Not found
      return;
    }
    if (toCompare == _lastReturned) {
      // We are done, eliminate
      _nextDepth.erase(_nextDepth.begin() + candidate);
      return;
    }
    if (toCompare < _lastReturned) {
      lowerBound = candidate;
    } else {
      upperBound = candidate;
    }
  }
}

arangodb::aql::AqlValue BreadthFirstEnumerator::lastVertexToAqlValue() {
  TRI_ASSERT(_lastReturned < _schreier.size());
  return _traverser->fetchVertexData(_schreier[_lastReturned]->vertex);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::lastEdgeToAqlValue() {
  TRI_ASSERT(_lastReturned < _schreier.size());
  if (_lastReturned == 0) {
    // This is the first Vertex. No Edge Pointing to it
    return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  }
  return _opts->cache()->fetchEdgeAqlResult(_schreier[_lastReturned]->edge);
}

arangodb::aql::AqlValue BreadthFirstEnumerator::pathToAqlValue(arangodb::velocypack::Builder& result) {
  // TODO make deque class variable
  std::deque<size_t> fullPath;
  size_t cur = _lastReturned;
  while (cur != 0) {
    // Walk backwards through the path and push everything found on the local
    // stack
    fullPath.emplace_front(cur);
    cur = _schreier[cur]->sourceIdx;
  }

  result.clear();
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& idx : fullPath) {
    _opts->cache()->insertEdgeIntoResult(_schreier[idx]->edge, result);
  }
  result.close();  // edges
  result.add(VPackValue("vertices"));
  result.openArray();
  // Always add the start vertex
  _traverser->addVertexToVelocyPack(_schreier[0]->vertex, result);
  for (auto const& idx : fullPath) {
    _traverser->addVertexToVelocyPack(_schreier[idx]->vertex, result);
  }
  result.close();  // vertices
  result.close();
  return arangodb::aql::AqlValue(result.slice());
}

bool BreadthFirstEnumerator::pathContainsVertex(size_t index, StringRef vertex) const {
  while (true) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    // Massive logic error, only valid pointers should be inserted into
    // _schreier
    TRI_ASSERT(step != nullptr);
    if (step->vertex == vertex) {
      // We have the given vertex on this path
      return true;
    }
    if (index == 0) {
      // We have checked the complete path
      return false;
    }
    index = step->sourceIdx;
  }
  return false;
}

bool BreadthFirstEnumerator::pathContainsEdge(size_t index,
                                              graph::EdgeDocumentToken const& edge) const {
  while (index != 0) {
    TRI_ASSERT(index < _schreier.size());
    auto const& step = _schreier[index];
    // Massive logic error, only valid pointers should be inserted into
    // _schreier
    TRI_ASSERT(step != nullptr);
    if (step->edge.equals(edge)) {
      // We have the given vertex on this path
      return true;
    }
    index = step->sourceIdx;
  }
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
