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
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeDocumentToken.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::graph;

ClusterTraverserCache::ClusterTraverserCache(
    transaction::Methods* trx,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines)
    : TraverserCache(trx), _engines(engines) {}

ClusterTraverserCache::~ClusterTraverserCache() {}

aql::AqlValue ClusterTraverserCache::fetchAqlResult(EdgeDocumentToken const* idToken) {
  // This cast is save because the Coordinator can only create those tokens
  auto tkn = static_cast<ClusterEdgeDocumentToken const*>(idToken);
  TRI_ASSERT(tkn != nullptr);
  return fetchAqlResult(tkn->id());
}

aql::AqlValue ClusterTraverserCache::fetchAqlResult(StringRef id) {
  auto it = _edges.find(id);
  if (it == _edges.end()) {
    // Document not found return NULL
    return AqlValue(VelocyPackHelper::NullValue());
  }
  return AqlValue(it->second);
}

void ClusterTraverserCache::insertIntoResult(StringRef id,
                                             VPackBuilder& result) {
  auto it = _edges.find(id);
  if (it == _edges.end()) {
    result.add(VelocyPackHelper::NullValue());
    return;
  }
  result.add(_edges[id]);
}

void ClusterTraverserCache::insertIntoResult(EdgeDocumentToken const* idToken,
                                             VPackBuilder& result) {
  // This cast is save because the Coordinator can only create those tokens
  auto tkn = static_cast<ClusterEdgeDocumentToken const*>(idToken);
  TRI_ASSERT(tkn != nullptr);
  insertIntoResult(tkn->id(), result);
}


std::unordered_map<StringRef, arangodb::velocypack::Slice>&
ClusterTraverserCache::edges() {
  return _edges;
}

std::vector<std::shared_ptr<arangodb::velocypack::Builder>>&
ClusterTraverserCache::datalake() {
  return _datalake;
}

std::unordered_map<ServerID, traverser::TraverserEngineID> const*
ClusterTraverserCache::engines() {
  return _engines;
}

size_t& ClusterTraverserCache::insertedDocuments() {
  return _insertedDocuments;
}

size_t& ClusterTraverserCache::filteredDocuments() {
  return _filteredDocuments;
}
