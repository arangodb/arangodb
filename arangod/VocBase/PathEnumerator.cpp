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
#include "Graph/TraverserCache.h"
#include "VocBase/Traverser.h"

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

    bool foundPath = false;

    auto callback = [&](std::unique_ptr<graph::EdgeDocumentToken>&& eid, VPackSlice const& edge,
                        size_t cursorId) {

      if (_opts->hasEdgeFilter(_enumeratedPath.edges.size(), cursorId)) {
        VPackSlice e = edge;
        if (edge.isString()) {
          e = _opts->cache()->lookupToken(eid.get());
        }
        if (!_traverser->edgeMatchesConditions(
              e, StringRef(_enumeratedPath.vertices.back()),
              _enumeratedPath.edges.size(), cursorId)) {
          // This edge does not pass the filtering
          return;
        }
      }

      if (_opts->uniqueEdges == TraverserOptions::UniquenessLevel::PATH) {
        for (auto const& it : _enumeratedPath.edges) {
          if (it->equals(eid.get())) {
            // We already have this edge on the path.
            return;
          }
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

        
        TRI_ASSERT(eid != nullptr);
        _enumeratedPath.edges.push_back(std::move(eid));
        foundPath = true;
        return;
      }
      // Vertex Invalid. Do neither insert edge nor vertex
    };

    while (!_edgeCursors.empty()) {
      TRI_ASSERT(_edgeCursors.size() == _enumeratedPath.edges.size() + 1);
      auto& cursor = _edgeCursors.top();

      if (cursor->next(callback)) {
        if (foundPath) {
          if (_enumeratedPath.edges.size() < _opts->minDepth) {
            // We have a valid prefix, but do NOT return this path
            break;
          }
          return true;
        }
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
  TRI_ASSERT(_enumeratedPath.edges.back() != nullptr);
  return _opts->cache()->fetchAqlResult(_enumeratedPath.edges.back().get());
}

arangodb::aql::AqlValue DepthFirstEnumerator::pathToAqlValue(
    arangodb::velocypack::Builder& result) {
  result.clear();
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _enumeratedPath.edges) {
    TRI_ASSERT(it != nullptr);
    _opts->cache()->insertIntoResult(it.get(), result);
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
