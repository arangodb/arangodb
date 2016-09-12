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
  if (_enumeratedPath.vertices.empty()) {
    // We are done;
    return false;
  }

  size_t cursorId = 0;

  while (true) {
    if (_enumeratedPath.edges.size() < _opts->maxDepth) {
      // We are not done with this path, so
      // we reserve the cursor for next depth
      auto cursor = _opts->nextCursor(_enumeratedPath.vertices.back(),
                                      _enumeratedPath.edges.size());
      if (cursor != nullptr) {
        _edgeCursors.emplace(cursor);
      }
    } else {
      if (!_enumeratedPath.edges.empty()) {
        // This path is at the end. cut the last step
        _enumeratedPath.vertices.pop_back();
        _enumeratedPath.edges.pop_back();
      }
    }

    while (!_edgeCursors.empty()) {
      TRI_ASSERT(_edgeCursors.size() == _enumeratedPath.edges.size() + 1);
      auto& cursor = _edgeCursors.top();
      if (cursor->next(_enumeratedPath.edges, cursorId)) {
        ++_traverser->_readDocuments;
        if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
          if (_returnedEdges.find(_enumeratedPath.edges.back()) ==
              _returnedEdges.end()) {
            // Edge not yet visited. Mark and continue.
            _returnedEdges.emplace(_enumeratedPath.edges.back());
          } else {
            _traverser->_filteredPaths++;
            TRI_ASSERT(!_enumeratedPath.edges.empty());
            _enumeratedPath.edges.pop_back();
            continue;
          }
        }
        if (!_traverser->edgeMatchesConditions(_enumeratedPath.edges.back(),
                                               _enumeratedPath.vertices.back(),
                                               _enumeratedPath.edges.size() - 1,
                                               cursorId)) {
            // This edge does not pass the filtering
            TRI_ASSERT(!_enumeratedPath.edges.empty());
            _enumeratedPath.edges.pop_back();
            continue;
        }

        if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
          auto& e = _enumeratedPath.edges.back();
          bool foundOnce = false;
          for (auto const& it : _enumeratedPath.edges) {
            if (foundOnce) {
              foundOnce = false; // if we leave with foundOnce == false we found the edge earlier
              break;
            }
            if (it == e) {
              foundOnce = true;
            }
          }
          if (!foundOnce) {
            // We found it and it was not the last element (expected)
            // This edge is allready on the path
            TRI_ASSERT(!_enumeratedPath.edges.empty());
            _enumeratedPath.edges.pop_back();
            continue;
          }
        }

        // We have to check if edge and vertex is valid
        if (_traverser->getVertex(_enumeratedPath.edges.back(),
                                  _enumeratedPath.vertices)) {
          // case both are valid.
          if (_opts->uniqueVertices == TraverserOptions::UniquenessLevel::PATH) {
            auto& e = _enumeratedPath.vertices.back();
            bool foundOnce = false;
            for (auto const& it : _enumeratedPath.vertices) {
              if (foundOnce) {
                foundOnce = false;  // if we leave with foundOnce == false we
                                    // found the edge earlier
                break;
              }
              if (it == e) {
                foundOnce = true;
              }
            }
            if (!foundOnce) {
              // We found it and it was not the last element (expected)
              // This vertex is allready on the path
              TRI_ASSERT(!_enumeratedPath.edges.empty());
              _enumeratedPath.vertices.pop_back();
              _enumeratedPath.edges.pop_back();
              continue;
            }
          }
          if (_enumeratedPath.edges.size() < _opts->minDepth) {
            // Do not return, but leave this loop. Continue with the outer.
            break;
          }

          return true;
        }
        // Vertex Invalid. Revoke edge
        TRI_ASSERT(!_enumeratedPath.edges.empty());
        _enumeratedPath.edges.pop_back();
        continue;
      } else {
        // cursor is empty.
        _edgeCursors.pop();
        if (!_enumeratedPath.edges.empty()) {
          _enumeratedPath.edges.pop_back();
          _enumeratedPath.vertices.pop_back();
        }
      }
    }
    if (_edgeCursors.empty()) {
      // If we get here all cursors are exhausted.
      _enumeratedPath.edges.clear();
      _enumeratedPath.vertices.clear();
      return false;
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
                                               VPackSlice startVertex,
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

    std::unique_ptr<arangodb::traverser::EdgeCursor> cursor(_opts->nextCursor(nextVertex, _currentDepth));
    if (cursor != nullptr) {
      size_t cursorIdx;
      bool shouldReturnPath = _currentDepth + 1 >= _opts->minDepth;
      bool didInsert = false;
      while (cursor->readAll(_tmpEdges, cursorIdx)) {
        if (!_tmpEdges.empty()) {
          _traverser->_readDocuments += _tmpEdges.size();
          VPackSlice v;
          for (auto const& e : _tmpEdges) {
            if (_opts->uniqueEdges ==
                TraverserOptions::UniquenessLevel::GLOBAL) {
              if (_returnedEdges.find(e) == _returnedEdges.end()) {
                // Edge not yet visited. Mark and continue.
                _returnedEdges.emplace(e);
              } else {
                _traverser->_filteredPaths++;
                continue;
              }
            }

            if (!_traverser->edgeMatchesConditions(e, nextVertex,
                                                   _currentDepth,
                                                   cursorIdx)) {
              continue;
            }
            if (_traverser->getSingleVertex(e, nextVertex, _currentDepth, v)) {
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
          _tmpEdges.clear();
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

