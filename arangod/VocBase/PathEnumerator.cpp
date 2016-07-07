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

#include "PathEnumerator.h"
#include "VocBase/Traverser.h"

using DepthFirstEnumerator = arangodb::traverser::DepthFirstEnumerator;
using BreadthFirstEnumerator = arangodb::traverser::BreadthFirstEnumerator;
using Traverser = arangodb::traverser::Traverser;
using TraverserOptions = arangodb::traverser::TraverserOptions;

bool DepthFirstEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (_opts->minDepth == 0) {
      return true;
    }
  }
  if (_enumeratedPath.edges.size() == _opts->maxDepth) {
    // we have reached the maximal search depth.
    // We can prune this path and go to the next.
    prune();
  }

  // Avoid tail recursion. May crash on high search depth
  while (true) {
    if (_lastEdges.empty()) {
      _enumeratedPath.edges.clear();
      _enumeratedPath.vertices.clear();
      return false;
    }
    _traverser->getEdge(_enumeratedPath.vertices.back(), _enumeratedPath.edges,
                        _lastEdges.top(), _lastEdgesIdx.top());
    if (_lastEdges.top() != nullptr) {
      // Could continue the path in the next depth.
      _lastEdges.push(nullptr);
      _lastEdgesIdx.push(0);
      std::string v;
      bool isValid = _traverser->getVertex(_enumeratedPath.edges.back(),
                                           _enumeratedPath.vertices.back(),
                                           _enumeratedPath.vertices.size(), v);
      _enumeratedPath.vertices.push_back(v);
      TRI_ASSERT(_enumeratedPath.vertices.size() ==
                 _enumeratedPath.edges.size() + 1);
      if (isValid) {
        if (_enumeratedPath.edges.size() < _opts->minDepth) {
          // The path is ok as a prefix. But to short to be returned.
          continue;
        }
        if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
          // it is sufficient to check if any of the vertices on the path is equal to the end.
          // Then we prune and any intermediate equality cannot happen.
          auto& last = _enumeratedPath.vertices.back();
          auto found = std::find(_enumeratedPath.vertices.begin(), _enumeratedPath.vertices.end(), last);
          TRI_ASSERT(found != _enumeratedPath.vertices.end()); // We have to find it once, it is at least the last!
          if ((++found) != _enumeratedPath.vertices.end()) {
            // Test if we found the last element. That is ok.
            prune();
            continue;
          }
        }
        return true;
      }
    } else {
      if (_enumeratedPath.edges.empty()) {
        // We are done with enumerating paths
        _enumeratedPath.edges.clear();
        _enumeratedPath.vertices.clear();
        return false;
      }
    }
    // This either modifies the stack or _lastEdges is empty.
    // This will return in next depth
    prune();
  }
}

void DepthFirstEnumerator::prune() {
  if (!_lastEdges.empty()) {
    _lastEdges.pop();
    _lastEdgesIdx.pop();
    if (!_enumeratedPath.edges.empty()) {
      _enumeratedPath.edges.pop_back();
      _enumeratedPath.vertices.pop_back();
    }
  }
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastVertexToAqlValue() {
  return _traverser->fetchVertexData(_enumeratedPath.vertices.back());
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastEdgeToAqlValue() {
  if (_enumeratedPath.edges.empty()) {
    return arangodb::aql::AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  return _traverser->fetchEdgeData(_enumeratedPath.edges.back());
}

arangodb::aql::AqlValue DepthFirstEnumerator::pathToAqlValue(arangodb::velocypack::Builder& result) {
  result.clear();
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _enumeratedPath.edges) {
    _traverser->addEdgeToVelocyPack(it, result);
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _enumeratedPath.vertices) {
    _traverser->addVertexToVelocyPack(it, result);
  }
  result.close();
  result.close();
  return arangodb::aql::AqlValue(result.slice());
}

BreadthFirstEnumerator::BreadthFirstEnumerator(Traverser* traverser,
                                               std::string const& startVertex,
                                               TraverserOptions const* opts)
    : PathEnumerator(traverser, startVertex, opts),
      _schreierIndex(1),
      _lastReturned(0),
      _currentDepth(0),
      _toSearchPos(0) {
  _schreier.reserve(32);
  auto step = std::make_unique<PathStep>(startVertex);
  _schreier.emplace_back(step.get());
  step.release();

  _toSearch.emplace_back(NextStep(0));
}

bool BreadthFirstEnumerator::next() {
  if (_isFirst) {
    _isFirst = false;
    if (_opts->minDepth == 0) {
      computeEnumeratedPath(_lastReturned++);
      return true;
    }
    _lastReturned++;
  }

  if (_lastReturned < _schreierIndex) {
    // We still have something on our stack.
    // Paths have been read but not returned.
    computeEnumeratedPath(_lastReturned++);
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
      if (_nextDepth.empty()) {
        // That's it. we are done.
        _enumeratedPath.edges.clear();
        _enumeratedPath.vertices.clear();
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
    }
    // This access is always safe.
    // If not it should have bailed out before.
    TRI_ASSERT(_toSearchPos < _toSearch.size());

    _tmpEdges.clear();
    auto const nextIdx = _toSearch[_toSearchPos++].sourceIdx;
    auto const& nextVertex = _schreier[nextIdx]->vertex;

    _traverser->getAllEdges(nextVertex, _tmpEdges, _currentDepth);
    bool shouldReturnPath = _currentDepth + 1 >= _opts->minDepth;
    if (!_tmpEdges.empty()) {
      bool didInsert = false;
      std::string v;
      for (auto const& e : _tmpEdges) {
        bool valid =
            _traverser->getVertex(e, nextVertex, _currentDepth, v);
        if (valid) {
          auto step = std::make_unique<PathStep>(nextIdx, e, v);
          _schreier.emplace_back(step.get());
          step.release();
          if (_currentDepth < _opts->maxDepth - 1) {
            _nextDepth.emplace_back(NextStep(_schreierIndex));
          }
          _schreierIndex++;
          didInsert = true;
        }
      }
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
  // entry. We compute the path to it
  // and increase the schreierIndex to point
  // to the next free position.
  computeEnumeratedPath(_lastReturned++);
  return true;
}

void BreadthFirstEnumerator::prune() {
  if (!_nextDepth.empty()) {
    _nextDepth.pop_back();
  }
}

// TODO Optimize this. Remove enumeratedPath
// All can be read from schreier vector directly
arangodb::aql::AqlValue BreadthFirstEnumerator::lastVertexToAqlValue() {
  return _traverser->fetchVertexData(
      _enumeratedPath.vertices.back());
}

arangodb::aql::AqlValue BreadthFirstEnumerator::lastEdgeToAqlValue() {
  if (_enumeratedPath.edges.empty()) {
    return arangodb::aql::AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  return _traverser->fetchEdgeData(_enumeratedPath.edges.back());
}

arangodb::aql::AqlValue BreadthFirstEnumerator::pathToAqlValue(
    arangodb::velocypack::Builder& result) {
  result.clear();
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _enumeratedPath.edges) {
    _traverser->addEdgeToVelocyPack(it, result);
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _enumeratedPath.vertices) {
    _traverser->addVertexToVelocyPack(it, result);
  }
  result.close();
  result.close();
  return arangodb::aql::AqlValue(result.slice());
}

void BreadthFirstEnumerator::computeEnumeratedPath(size_t index) {
  TRI_ASSERT(index < _schreier.size());

  size_t depth = getDepth(index);
  _enumeratedPath.edges.clear();
  _enumeratedPath.vertices.clear();
  _enumeratedPath.edges.resize(depth);
  _enumeratedPath.vertices.resize(depth + 1);

  // Computed path. Insert it into the path enumerator.
  PathStep* current = nullptr;
  while (index != 0) {
    TRI_ASSERT(depth > 0);
    current = _schreier[index];
    _enumeratedPath.vertices[depth] = current->vertex;
    _enumeratedPath.edges[depth - 1] = current->edge;

    index = current->sourceIdx;
    --depth;
  }

  current = _schreier[0];
  _enumeratedPath.vertices[0] = current->vertex;
}

