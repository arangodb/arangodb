////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2017 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterTraverserCache.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeDocumentToken.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;

ClusterTraverserCache::ClusterTraverserCache(
    aql::Query* query, std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines)
    : TraverserCache(query), _engines(engines) {}

VPackSlice ClusterTraverserCache::lookupToken(EdgeDocumentToken const& token) {
  return VPackSlice(token.vpack());
}

aql::AqlValue ClusterTraverserCache::fetchEdgeAqlResult(EdgeDocumentToken const& token) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  // FIXME: the ClusterTraverserCache lifetime is shorter then the query
  // lifetime therefore we cannot get away here without copying the result
  return aql::AqlValue(VPackSlice(token.vpack()));  // will copy slice
}

aql::AqlValue ClusterTraverserCache::fetchVertexAqlResult(arangodb::velocypack::StringRef id) {
  // FIXME: this is only used for ShortestPath, where the shortestpath stuff
  // uses _edges to store its vertices
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  auto it = _cache.find(id);
  if (it == _cache.end()) {
    // Register a warning. It is okay though but helps the user
    std::string msg = "vertex '" + id.toString() + "' not found";
    _query->registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

    // Document not found return NULL
    return aql::AqlValue(aql::AqlValueHintNull());
  }
  // FIXME: the ClusterTraverserCache lifetime is shorter then the query
  // lifetime therefore we cannot get away here without copying the result
  return aql::AqlValue(it->second);  // will copy slice
}

void ClusterTraverserCache::insertEdgeIntoResult(EdgeDocumentToken const& token,
                                                 VPackBuilder& result) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  result.add(VPackSlice(token.vpack()));
}

void ClusterTraverserCache::insertVertexIntoResult(arangodb::velocypack::StringRef id, VPackBuilder& result) {
  auto it = _cache.find(id);
  if (it == _cache.end()) {
    // Register a warning. It is okay though but helps the user
    std::string msg = "vertex '" + id.toString() + "' not found";
    _query->registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());
    // Document not found append NULL
    result.add(arangodb::velocypack::Slice::nullSlice());
  } else {
    // FIXME: fix TraverserCache lifetime and use addExternal
    result.add(it->second);
  }
}
