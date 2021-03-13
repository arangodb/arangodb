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

#include "ClusterEdgeCursor.h"

#include "Basics/Result.h"
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

ClusterEdgeCursor::ClusterEdgeCursor(graph::BaseOptions const* opts) 
    : _position(0),
      _opts(opts),
      _cache(static_cast<ClusterTraverserCache*>(opts->cache())),
      _httpRequests(0) {
  TRI_ASSERT(_cache != nullptr);

  if (_cache == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "no cache present for cluster edge cursor");
  }
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
  
ClusterTraverserEdgeCursor::ClusterTraverserEdgeCursor(traverser::TraverserOptions const* opts)
    : ClusterEdgeCursor(opts) {}

traverser::TraverserOptions const* ClusterTraverserEdgeCursor::traverserOptions() const {
  TRI_ASSERT(dynamic_cast<traverser::TraverserOptions const*>(_opts) != nullptr);
  return dynamic_cast<traverser::TraverserOptions const*>(_opts);
}

void ClusterTraverserEdgeCursor::rearm(arangodb::velocypack::StringRef vertexId, uint64_t depth) {
  _edgeList.clear();
  _position = 0;

  auto trx = _opts->trx();
  TRI_ASSERT(trx != nullptr);
  TRI_ASSERT(_cache != nullptr);

  Result res = fetchEdgesFromEngines(*trx, *_cache, traverserOptions()->getExpressionCtx(), vertexId, depth, _edgeList);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  _httpRequests += _cache->engines()->size();
}

ClusterShortestPathEdgeCursor::ClusterShortestPathEdgeCursor(graph::BaseOptions const* opts, bool backward)
    : ClusterEdgeCursor(opts),
      _backward(backward) {}

void ClusterShortestPathEdgeCursor::rearm(arangodb::velocypack::StringRef vertexId, uint64_t /*depth*/) {
  _edgeList.clear();
  _position = 0;

  auto trx = _opts->trx();
  transaction::BuilderLeaser b(trx);

  b->add(VPackValuePair(vertexId.data(), vertexId.length(), VPackValueType::String));
  Result res = fetchEdgesFromEngines(*trx, *_cache, b->slice(), _backward, _edgeList, _cache->insertedDocuments());
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  _httpRequests += _cache->engines()->size();
}
