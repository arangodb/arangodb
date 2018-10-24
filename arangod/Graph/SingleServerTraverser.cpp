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
#include "Basics/StringRef.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/NeighborsEnumerator.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::graph;

SingleServerTraverser::SingleServerTraverser(TraverserOptions* opts,
                                             transaction::Methods* trx,
                                             ManagedDocumentResult* mmdr)
  : Traverser(opts, trx, mmdr) {}

SingleServerTraverser::~SingleServerTraverser() {}

void SingleServerTraverser::addVertexToVelocyPack(StringRef vid,
                                                  VPackBuilder& result) {
  _opts->cache()->insertVertexIntoResult(vid, result);
}

aql::AqlValue SingleServerTraverser::fetchVertexData(StringRef vid) {
  return _opts->cache()->fetchVertexAqlResult(vid);
}

void SingleServerTraverser::setStartVertex(std::string const& vid) {
  _startIdBuilder.clear();
  _startIdBuilder.add(VPackValue(vid));
  VPackSlice idSlice = _startIdBuilder.slice();

  if (!vertexMatchesConditions(StringRef(vid), 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }

  StringRef persId = _opts->cache()->persistString(StringRef(vid));
  _vertexGetter->reset(persId);

  if (_opts->useBreadthFirst) {
    if (_canUseOptimizedNeighbors) {
      _enumerator.reset(new NeighborsEnumerator(this, idSlice, _opts));
    } else {
      _enumerator.reset(new BreadthFirstEnumerator(this, idSlice, _opts));
    }
  } else {
    _enumerator.reset(new DepthFirstEnumerator(this, vid, _opts));
  }
  _done = false;
}

bool SingleServerTraverser::getVertex(VPackSlice edge,
                                      std::vector<StringRef>& result) {
  return _vertexGetter->getVertex(edge, result);
}

bool SingleServerTraverser::getSingleVertex(VPackSlice edge, StringRef const sourceVertexId,
                                            uint64_t depth, StringRef& targetVertexId) {
  return _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
}
