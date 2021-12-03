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

#include "SingleServerTraverser.h"
#include "Aql/AqlValue.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/EdgeCursor.h"
#include "Graph/NeighborsEnumerator.h"
#include "Graph/PathEnumerator.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Graph/WeightedEnumerator.h"
#include "Transaction/Methods.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::graph;

SingleServerTraverser::SingleServerTraverser(TraverserOptions* opts)
    : Traverser(opts) {
  createEnumerator();
}

SingleServerTraverser::~SingleServerTraverser() = default;

void SingleServerTraverser::addVertexToVelocyPack(std::string_view vid,
                                                  VPackBuilder& result) {
  _opts->cache()->appendVertex(vid, result);
}

aql::AqlValue SingleServerTraverser::fetchVertexData(std::string_view vid) {
  arangodb::aql::AqlValue result;
  _opts->cache()->appendVertex(vid, result);
  return result;
}

void SingleServerTraverser::setStartVertex(std::string const& vid) {
  std::string_view const s(vid);
  if (!vertexMatchesConditions(s, 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }

  std::string_view persId = _opts->cache()->persistString(s);
  _vertexGetter->reset(persId);
  _enumerator->setStartVertex(persId);
  _done = false;
}

void SingleServerTraverser::clear() {
  _vertexGetter->clear();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_vertexGetter->pointsIntoTraverserCache());
#endif
  _enumerator->clear();
  traverserCache()->clear();
}

bool SingleServerTraverser::getVertex(VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  return _vertexGetter->getVertex(edge, path);
}

bool SingleServerTraverser::getSingleVertex(VPackSlice edge,
                                            std::string_view sourceVertexId,
                                            uint64_t depth,
                                            std::string_view& targetVertexId) {
  return _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
}

void SingleServerTraverser::createEnumerator() {
  TRI_ASSERT(_enumerator == nullptr);

  switch (_opts->mode) {
    case TraverserOptions::Order::DFS:
      TRI_ASSERT(!_opts->useNeighbors);
      // normal, depth-first enumerator
      _enumerator = std::make_unique<DepthFirstEnumerator>(this, _opts);
      break;
    case TraverserOptions::Order::BFS:
      if (_opts->useNeighbors) {
        // optimized neighbors enumerator
        _enumerator = std::make_unique<NeighborsEnumerator>(this, _opts);
      } else {
        // default breadth-first enumerator
        _enumerator = std::make_unique<BreadthFirstEnumerator>(this, _opts);
      }
      break;
    case TraverserOptions::Order::WEIGHTED:
      TRI_ASSERT(!_opts->useNeighbors);
      _enumerator = std::make_unique<WeightedEnumerator>(this, _opts);
      break;
  }
}

bool SingleServerTraverser::getVertex(std::string_view vertex, size_t depth) {
  return _vertexGetter->getVertex(vertex, depth);
}
