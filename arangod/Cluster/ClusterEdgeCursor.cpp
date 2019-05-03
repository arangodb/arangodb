////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterEdgeCursor.h"

#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterTraverser.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

// Traverser variant
ClusterEdgeCursor::ClusterEdgeCursor(arangodb::velocypack::StringRef vertexId, uint64_t depth,
                                     graph::BaseOptions* opts)
    : _position(0),
      _resolver(opts->trx()->resolver()),
      _opts(opts),
      _cache(static_cast<ClusterTraverserCache*>(opts->cache())),
      _httpRequests(0) {
  TRI_ASSERT(_cache != nullptr);
  auto trx = _opts->trx();
  transaction::BuilderLeaser leased(trx);
  transaction::BuilderLeaser b(trx);

  b->add(VPackValuePair(vertexId.data(), vertexId.length(), VPackValueType::String));
  fetchEdgesFromEngines(trx->vocbase().name(), _cache->engines(), b->slice(), depth,
                        _cache->cache(), _edgeList, _cache->datalake(), *(leased.get()),
                        _cache->filteredDocuments(), _cache->insertedDocuments());
  _httpRequests += _cache->engines()->size();
}

// ShortestPath variant
ClusterEdgeCursor::ClusterEdgeCursor(arangodb::velocypack::StringRef vertexId, bool backward, graph::BaseOptions* opts)
    : _position(0),
      _resolver(opts->trx()->resolver()),
      _opts(opts),
      _cache(static_cast<ClusterTraverserCache*>(opts->cache())),
      _httpRequests(0) {
  TRI_ASSERT(_cache != nullptr);
  auto trx = _opts->trx();
  transaction::BuilderLeaser leased(trx);
  transaction::BuilderLeaser b(trx);

  b->add(VPackValuePair(vertexId.data(), vertexId.length(), VPackValueType::String));
  fetchEdgesFromEngines(trx->vocbase().name(), _cache->engines(), b->slice(),
                        backward, _cache->cache(), _edgeList, _cache->datalake(),
                        *(leased.get()), _cache->insertedDocuments());
  _httpRequests += _cache->engines()->size();
}

bool ClusterEdgeCursor::next(EdgeCursor::Callback const& callback) {
  if (_position < _edgeList.size()) {
    VPackSlice edge = _edgeList[_position];
    callback(EdgeDocumentToken(edge), edge, _position);
    ++_position;
    return true;
  }
  return false;
}

void ClusterEdgeCursor::readAll(EdgeCursor::Callback const& callback) {
  for (VPackSlice const& edge : _edgeList) {
    callback(EdgeDocumentToken(edge), edge, _position);
  }
}
