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
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeCursor.h"
#include "VocBase/Traverser.h"
#include "VocBase/TraverserCache.h"

using PathEnumerator = arangodb::traverser::PathEnumerator;
using DepthFirstEnumerator = arangodb::traverser::DepthFirstEnumerator;
using Traverser = arangodb::traverser::Traverser;
using TraverserOptions = arangodb::traverser::TraverserOptions;

PathEnumerator::PathEnumerator(Traverser* traverser,
                               std::string const& startVertex,
                               TraverserOptions* opts)
    : _traverser(traverser), _isFirst(true), _opts(opts) {
  StringRef svId = _opts->cache()->persistString(StringRef(startVertex));
  // Guarantee that this vertex _id does not run away
  _enumeratedPath.vertices.push_back(svId);
  TRI_ASSERT(_enumeratedPath.vertices.size() == 1);
}

DepthFirstEnumerator::DepthFirstEnumerator(Traverser* traverser,
                                           std::string const& startVertex,
                                           TraverserOptions* opts)
    : PathEnumerator(traverser, startVertex, opts) {}

DepthFirstEnumerator::~DepthFirstEnumerator() {}

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

  while (true) {
    if (_enumeratedPath.edges.size() < _opts->maxDepth) {
      // We are not done with this path, so
      // we reserve the cursor for next depth
      auto cursor = _opts->nextCursor(
          _traverser->mmdr(), StringRef(_enumeratedPath.vertices.back()),
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

      bool foundPath = false;
      bool exitInnerLoop = false;
      auto callback = [&](StringRef const& eid, VPackSlice const& edge,
                          size_t cursorId) {
        _enumeratedPath.edges.push_back(eid);

        if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::GLOBAL) {
          if (_returnedEdges.find(eid) == _returnedEdges.end()) {
            // Edge not yet visited. Mark and continue.
            _returnedEdges.emplace(eid);
          } else {
            _opts->cache()->increaseFilterCounter();
            TRI_ASSERT(!_enumeratedPath.edges.empty());
            _enumeratedPath.edges.pop_back();
            return;
          }
        }
        if (!_traverser->edgeMatchesConditions(
                edge, StringRef(_enumeratedPath.vertices.back()),
                _enumeratedPath.edges.size() - 1, cursorId)) {
          // This edge does not pass the filtering
          TRI_ASSERT(!_enumeratedPath.edges.empty());
          _enumeratedPath.edges.pop_back();
          return;
        }

        if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
          StringRef const& e = _enumeratedPath.edges.back();
          bool foundOnce = false;
          for (StringRef const& it : _enumeratedPath.edges) {
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
            // This edge is allready on the path
            TRI_ASSERT(!_enumeratedPath.edges.empty());
            _enumeratedPath.edges.pop_back();
            return;
          }
        }

        // We have to check if edge and vertex is valid
        if (_traverser->getVertex(edge, _enumeratedPath.vertices)) {
          // case both are valid.
          if (_opts->uniqueVertices ==
              TraverserOptions::UniquenessLevel::PATH) {
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
              return;
            }
          }
          if (_enumeratedPath.edges.size() < _opts->minDepth) {
            // Do not return, but leave this loop. Continue with the outer.
            exitInnerLoop = true;
            return;
          }

          foundPath = true;
          return;
        }
        // Vertex Invalid. Revoke edge
        TRI_ASSERT(!_enumeratedPath.edges.empty());
        _enumeratedPath.edges.pop_back();
      };
      if (cursor->next(callback)) {
        if (foundPath) {
          return true;
        } else if (exitInnerLoop) {
          break;
        }
      } else {
        // cursor is empty.
        _edgeCursors.pop();
        if (!_enumeratedPath.edges.empty()) {
          _enumeratedPath.edges.pop_back();
          _enumeratedPath.vertices.pop_back();
        }
      }
    }  // while (!_edgeCursors.empty())
    if (_edgeCursors.empty()) {
      // If we get here all cursors are exhausted.
      _enumeratedPath.edges.clear();
      _enumeratedPath.vertices.clear();
      return false;
    }
  }  // while (true)
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastVertexToAqlValue() {
  return _traverser->fetchVertexData(
      StringRef(_enumeratedPath.vertices.back()));
}

arangodb::aql::AqlValue DepthFirstEnumerator::lastEdgeToAqlValue() {
  if (_enumeratedPath.edges.empty()) {
    return arangodb::aql::AqlValue(
        arangodb::basics::VelocyPackHelper::NullValue());
  }
  return _traverser->fetchEdgeData(StringRef(_enumeratedPath.edges.back()));
}

arangodb::aql::AqlValue DepthFirstEnumerator::pathToAqlValue(
    arangodb::velocypack::Builder& result) {
  result.clear();
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _enumeratedPath.edges) {
    _traverser->addEdgeToVelocyPack(StringRef(it), result);
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _enumeratedPath.vertices) {
    _traverser->addVertexToVelocyPack(StringRef(it), result);
  }
  result.close();
  result.close();
  return arangodb::aql::AqlValue(result.slice());
}
