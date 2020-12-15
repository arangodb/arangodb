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

#include "SingleServerTraverser.h"
#include "Aql/AqlValue.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/EdgeCursor.h"
#include "Graph/NeighborsEnumerator.h"
#include "Graph/PathEnumerator.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Methods.h"

#include <velocypack/StringRef.h>

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::graph;

SingleServerTraverser::SingleServerTraverser(TraverserOptions* opts)
    : Traverser(opts) {
  createEnumerator();
}

SingleServerTraverser::~SingleServerTraverser() = default;

void SingleServerTraverser::addVertexToVelocyPack(arangodb::velocypack::StringRef vid, VPackBuilder& result) {
  _opts->cache()->insertVertexIntoResult(vid, result);
}

aql::AqlValue SingleServerTraverser::fetchVertexData(arangodb::velocypack::StringRef vid) {
  return _opts->cache()->fetchVertexAqlResult(vid);
}

void SingleServerTraverser::setStartVertex(std::string const& vid) {
  arangodb::velocypack::StringRef const s(vid);
  if (!vertexMatchesConditions(s, 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }
  
  arangodb::velocypack::StringRef persId = _opts->cache()->persistString(s);
  _vertexGetter->reset(persId);
  _enumerator->setStartVertex(persId);
  _done = false;
}

void SingleServerTraverser::clear() {
  traverserCache()->clear();
}

bool SingleServerTraverser::getVertex(VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  return _vertexGetter->getVertex(edge, path);
}

bool SingleServerTraverser::getSingleVertex(VPackSlice edge, arangodb::velocypack::StringRef sourceVertexId,
                                            uint64_t depth, arangodb::velocypack::StringRef& targetVertexId) {
  return _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
}

void SingleServerTraverser::createEnumerator() {
  TRI_ASSERT(_enumerator == nullptr);

  if (_opts->useBreadthFirst) {
    // breadth-first enumerator
    if (_opts->useNeighbors) {
      // optimized neighbors enumerator
      _enumerator.reset(new NeighborsEnumerator(this, _opts));
    } else {
      // default breadth-first enumerator
      _enumerator.reset(new BreadthFirstEnumerator(this, _opts));
    }
  } else {
    // normal, depth-first enumerator
    _enumerator.reset(new DepthFirstEnumerator(this, _opts));
  }
}
