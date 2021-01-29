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

#include "RefactoredClusterTraverserCache.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Basics/ResourceUsage.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;

RefactoredClusterTraverserCache::RefactoredClusterTraverserCache(
    aql::QueryContext& query,
    std::unordered_map<ServerID, aql::EngineId> const* engines, BaseOptions* options)
    : TraverserCache(query, options),
      _datalake(options->resourceMonitor()),
      _engines(engines) {}

VPackSlice RefactoredClusterTraverserCache::lookupToken(EdgeDocumentToken const& token) {
  return VPackSlice(token.vpack());
}

aql::AqlValue RefactoredClusterTraverserCache::fetchEdgeAqlResult(EdgeDocumentToken const& token) {
  // FIXME: the RefactoredClusterTraverserCache lifetime is shorter than the
  // query lifetime therefore we cannot get away here without copying the result
  return aql::AqlValue(VPackSlice(token.vpack()));  // will copy slice
}

void RefactoredClusterTraverserCache::insertEdgeIntoResult(EdgeDocumentToken const& token,
                                                           VPackBuilder& result) {
  // TODO: to be removed / changed
  // result.add(getCachedEdge(token.localDocumentId().));
}

bool RefactoredClusterTraverserCache::appendVertex(arangodb::velocypack::StringRef id,
                                                   VPackBuilder& result) {
  // There will be no idString of length above uint32_t
  auto it = _cache.find(
      arangodb::velocypack::HashedStringRef(id.data(), static_cast<uint32_t>(id.length())));

  if (it != _cache.end()) {
    // FIXME: fix TraverserCache lifetime and use addExternal
    result.add(it->second);
    return true;
  }
  // Register a warning. It is okay though but helps the user
  std::string msg = "vertex '" + id.toString() + "' not found";
  _query.warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

  // Document not found append NULL
  result.add(arangodb::velocypack::Slice::nullSlice());
  return false;
}

bool RefactoredClusterTraverserCache::appendVertex(arangodb::velocypack::StringRef id,
                                                   arangodb::aql::AqlValue& result) {
  // There will be no idString of length above uint32_t
  auto it = _cache.find(
      arangodb::velocypack::HashedStringRef(id.data(), static_cast<uint32_t>(id.length())));

  if (it != _cache.end()) {
    // FIXME: fix TraverserCache lifetime and use addExternal
    result = arangodb::aql::AqlValue(it->second);
    return true;
  }
  // Register a warning. It is okay though but helps the user
  std::string msg = "vertex '" + id.toString() + "' not found";
  _query.warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

  // Document not found append NULL
  result = arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
  return false;
}

auto RefactoredClusterTraverserCache::cacheVertex(VertexType const& vertexId,
                                                  velocypack::Slice vertexSlice,
                                                  bool backward) -> void {
  if (backward) {
    _vertexData.try_emplace(vertexId, vertexSlice);
  } else {
    _vertexData.try_emplace(vertexId, vertexSlice);
  }
}

namespace {
VertexType getEdgeDestination(arangodb::velocypack::Slice edge, VertexType origin) {
  if (edge.isString()) {
    return VertexType{edge};
  }

  TRI_ASSERT(edge.isObject());
  auto from = edge.get(arangodb::StaticStrings::FromString);
  TRI_ASSERT(from.isString());
  if (from.stringRef() == origin.stringRef()) {
    auto to = edge.get(arangodb::StaticStrings::ToString);
    TRI_ASSERT(to.isString());
    return VertexType{to};
  }
  return VertexType{from};
}
}  // namespace

auto RefactoredClusterTraverserCache::cacheEdge(VertexType origin, EdgeType edgeId,
                                                velocypack::Slice edgeSlice,
                                                bool backward) -> void {
  TRI_ASSERT(isVertexCached(origin, backward));

  auto edgeToEmplace =
      std::pair{edgeId, VertexType{getEdgeDestination(edgeSlice, origin)}};
  std::vector<std::pair<EdgeType, VertexType>> initVector{};

  // TODO: in the future handle ANY as well
  if (backward) {
    _edgeDataBackward.try_emplace(edgeId, edgeSlice);
    _vertexConnectedEdgesBackward.try_emplace(origin, initVector); // TODO inert only if needed (better init)
    auto& edgeVectorBackward = _vertexConnectedEdgesBackward.at(origin);
    edgeVectorBackward.emplace_back(std::move(edgeToEmplace));
  } else {
    _edgeDataForward.try_emplace(edgeId, edgeSlice);
    _vertexConnectedEdgesForward.try_emplace(origin, initVector); // TODO insert only if needed (better init)
    auto& edgeVectorForward = _vertexConnectedEdgesForward.at(origin);
    edgeVectorForward.emplace_back(std::move(edgeToEmplace));
  }
}

auto RefactoredClusterTraverserCache::getVertexRelations(const VertexType& vertex, bool backward)
    -> std::vector<std::pair<EdgeType, VertexType>> {
  TRI_ASSERT(isVertexCached(vertex, backward));

  if (!isVertexRelationCached(vertex, backward)) {
    return {};
  }

  if (backward) {
    return _vertexConnectedEdgesBackward.at(vertex);
  }

  return _vertexConnectedEdgesForward.at(vertex);
}

auto RefactoredClusterTraverserCache::isVertexCached(VertexType vertexKey, bool backward)
    -> bool {
  if (backward) {
    return (_vertexData.find(vertexKey) == _vertexData.end()) ? false : true;
  }
  return (_vertexData.find(vertexKey) == _vertexData.end()) ? false : true;
}

auto RefactoredClusterTraverserCache::isVertexRelationCached(VertexType vertexKey,
                                                             bool backward) -> bool {
  if (backward) {
    return (_vertexConnectedEdgesBackward.find(vertexKey) ==
            _vertexConnectedEdgesBackward.end())
               ? false
               : true;
  }
  return (_vertexConnectedEdgesForward.find(vertexKey) ==
          _vertexConnectedEdgesForward.end())
             ? false
             : true;
}

auto RefactoredClusterTraverserCache::isEdgeCached(EdgeType edgeKey, bool backward) -> bool {
  if (backward) {
    return (_edgeDataBackward.find(edgeKey) == _edgeDataBackward.end()) ? false : true;
  }
  return (_edgeDataForward.find(edgeKey) == _edgeDataForward.end()) ? false : true;
}

auto RefactoredClusterTraverserCache::getCachedVertex(VertexType vertex, bool backward)
    -> VPackSlice {
  if (!isVertexCached(vertex, backward)) {
    return VPackSlice::noneSlice();
  }
  if (backward) {
    return _vertexData.at(vertex);
  }
  return _vertexData.at(vertex);
}

auto RefactoredClusterTraverserCache::getCachedEdge(EdgeType edge, bool backward) -> VPackSlice {
  if (!isEdgeCached(edge, backward)) {
    return VPackSlice::noneSlice();
  }
  if (backward) {
    return _edgeDataBackward.at(edge);
  }
  return _edgeDataForward.at(edge);
}